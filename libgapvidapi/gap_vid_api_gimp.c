/* gap_vid_api_gimp.c
 *
 * GAP Video read API implementation for singleframe sequence
 * wrappers for gap/gimp generic file load procedure
 *
 * please note that the singleframes are loaded as flatened frames
 * when they are read by this API.
 *
 * 2003.05.09   hof created
 *
 */


/* ================================================ gimp
 * gimp (singleframe access via gap/gimp)           gimp
 * ================================================ gimp
 * ================================================ gimp
 */
#ifdef ENABLE_GVA_GIMP

static gchar  *global_gva_gimp_filename = NULL;
static gint32  global_gva_gimp_image_id = -1;

/* -------------------------
 * API READ extension GIMP
 * -------------------------
 */

/* structure with gap specific anim information */
typedef struct t_GVA_gimp
{
   gint32       initial_image_id;
   long         first_frame_nr;
   long         last_frame_nr;
   long         curr_frame_nr;
   long         frame_cnt;
   char         basename[1024];    /* may include path */
   char         extension[50];
   gdouble      framerate;
} t_GVA_gimp;



/* -----------------------------
 * p_wrapper_gimp_check_sig
 * -----------------------------
 */
gboolean
p_wrapper_gimp_check_sig(char *filename)
{
  gint32 l_image_id;

  if(gap_debug) printf("p_wrapper_gimp_check_sig: START filename:%s\n", filename);

  l_image_id = gimp_file_load(GIMP_RUN_NONINTERACTIVE, filename, filename);
  if (l_image_id < 0)
  {
     if(gap_debug)
     {
       printf("p_wrapper_gimp_check_sig:%s: could not load file\n", filename);
     }
     return(FALSE);
  }

  /* we do not delete l_image_id at this point.
   * in most cases the check_sig call is followed by open_read
   * where we need that image again
   */
  if(global_gva_gimp_filename)
  {
    g_free(global_gva_gimp_filename);
  }
  global_gva_gimp_filename = g_strdup(filename);
  global_gva_gimp_image_id = l_image_id;

  if(gap_debug) printf("p_wrapper_gimp_check_sig: compatible is TRUE\n");

  return (TRUE);
}

/* -----------------------------
 * p_wrapper_gimp_open_read
 * -----------------------------
 *  open performs a GAP directory scan to findout
 *  how many frames we have.
 *  the initial image (the one with filename)
 *  will be loaded as gimp_image and kept cached until close.
 */
void
p_wrapper_gimp_open_read(char *filename, t_GVA_Handle *gvahand)
{
  t_GVA_gimp*  handle;
  t_GVA_DecoderElem  *dec_elem;
  gint32       l_image_id;

  if(gap_debug) printf("p_wrapper_gimp_open_read: START filename:%s\n", filename);

  gvahand->decoder_handle = (void *)NULL;

  handle = g_malloc0(sizeof(t_GVA_gimp));

  l_image_id = -1;
  if(global_gva_gimp_filename)
  {
    if(strcmp(global_gva_gimp_filename, filename) == 0)
    {
      l_image_id = global_gva_gimp_image_id;
    }
  }

  if(l_image_id < 0)
  {
    l_image_id = gimp_file_load(GIMP_RUN_NONINTERACTIVE, filename, filename);
    if (l_image_id < 0)
    {
      g_free(handle);
      return;
    }
  }

  /* the initial image is now flattened and kept until close
   * (read ops for the curr_frame_nr will read from the initial_image_id
   *  and can skip the slower file load)
   */
  gimp_image_flatten(l_image_id);
  handle->initial_image_id = l_image_id;

  gvahand->gva_thread_save = FALSE;  /* calls to gimp from 2 threads do crash */
  gvahand->vtracks = 1;
  gvahand->atracks = 0;
  gvahand->frame_bpp = 3;              /* RGB pixelformat */
  gvahand->total_aud_samples = 0;
  gvahand->width = gimp_image_width(l_image_id);
  gvahand->height = gimp_image_height(l_image_id);

  /* fetch animframe informations (perform GAP directoryscan) */
  if(l_image_id >= 0)
  {
    static char     *l_called_proc = "plug_in_gap_get_animinfo";
    GimpParam       *return_vals;
    int              nreturn_vals;
    gint32           dummy_layer_id;

    dummy_layer_id = gap_image_get_any_layer(l_image_id);
    return_vals = gimp_run_procedure (l_called_proc,
                                 &nreturn_vals,
                                 GIMP_PDB_INT32, GIMP_RUN_NONINTERACTIVE,
                                 GIMP_PDB_IMAGE, l_image_id,
                                 GIMP_PDB_DRAWABLE, dummy_layer_id,
                                 GIMP_PDB_END);

    if (return_vals[0].data.d_status != GIMP_PDB_SUCCESS)
    {
      printf("Error: PDB call of %s failed, image_ID: %d\n", l_called_proc, (int)l_image_id);
      g_free(handle);
      return;
    }
    handle->first_frame_nr = return_vals[1].data.d_int32;
    handle->last_frame_nr =  return_vals[2].data.d_int32;
    handle->curr_frame_nr =  return_vals[3].data.d_int32;
    handle->frame_cnt =      return_vals[4].data.d_int32;
    g_snprintf(handle->basename, sizeof(handle->basename), "%s", return_vals[5].data.d_string);
    g_snprintf(handle->extension, sizeof(handle->extension), "%s", return_vals[6].data.d_string);
    handle->framerate = return_vals[7].data.d_float;
  }

  gvahand->framerate    = handle->framerate;
  gvahand->total_frames = handle->frame_cnt;
  gvahand->all_frames_counted = TRUE;  /* handle->frame_cnt is the exact total_frames number */

  dec_elem = (t_GVA_DecoderElem *)gvahand->dec_elem;

  if(dec_elem)
  {
    if(dec_elem->decoder_description)
    {
      g_free(dec_elem->decoder_description);
    }
    dec_elem->decoder_description =
        g_strdup_printf("GAP Decoder (for all GIMP readable imagefiles)\n"
                        " (EXT: .xcf,.jpg,.gif,.png,.tif,...)"
                       );
  }


  gvahand->decoder_handle = (void *)handle;

  if(gap_debug) printf("p_wrapper_gimp_open_read: END, OK\n");

}  /* end p_wrapper_gimp_open_read */


/* -----------------------------
 * p_wrapper_gimp_close
 * -----------------------------
 */
void
p_wrapper_gimp_close(t_GVA_Handle *gvahand)
{
  t_GVA_gimp *handle;

  if(gap_debug) printf("p_wrapper_gimp_close: START\n");

  handle = (t_GVA_gimp *)gvahand->decoder_handle;

  /* delete the initial image at close */
  p_gimp_image_delete(handle->initial_image_id);

  return;
}  /* end p_wrapper_gimp_close */


/* ----------------------------------
 * p_wrapper_gimp_get_next_frame
 * ----------------------------------
 * read singleframe with current_seek_nr and advance position
 * TODO: EOF detection
 *
 */
t_GVA_RetCode
p_wrapper_gimp_get_next_frame(t_GVA_Handle *gvahand)
{
  t_GVA_gimp *handle;
  gchar    *l_framename;
  gint32    l_image_id;
  gint32    l_frame_nr;
  t_GVA_RetCode       l_rc;

  handle = (t_GVA_gimp *)gvahand->decoder_handle;

  l_frame_nr = gvahand->current_seek_nr + (handle->first_frame_nr - 1);

  if(1==0  /*l_frame_nr == handle->curr_frame_nr*/)
  {
    /* this frame_nr is the initial_image, that is always cached
     * as gimp image (without a display)
     * no need to load agian, just transfer to rowbuffer
     */
    l_rc = GVA_gimp_image_to_rowbuffer(gvahand, handle->initial_image_id);
  }
  else
  {
    l_framename = p_alloc_fname(handle->basename
                               ,l_frame_nr
                               ,handle->extension
                               );

    l_image_id = gimp_file_load(GIMP_RUN_NONINTERACTIVE, l_framename, l_framename);
    if(l_image_id < 0)
    {
      l_rc = GVA_RET_ERROR;
    }
    else
    {
      l_rc = GVA_gimp_image_to_rowbuffer(gvahand, l_image_id);
      p_gimp_image_delete(l_image_id);
    }

    g_free(l_framename);
  }

  if (l_rc == GVA_RET_OK)
  {
    gvahand->current_frame_nr = gvahand->current_seek_nr;
    gvahand->current_seek_nr++;
    return(GVA_RET_OK);
  }

  return(l_rc);
}  /* end p_wrapper_gimp_get_next_frame */


/* ------------------------------
 * p_wrapper_gimp_seek_frame
 * ------------------------------
 *  - for the singleframe decoder
 *    we just set the current framenumber.
 */
t_GVA_RetCode
p_wrapper_gimp_seek_frame(t_GVA_Handle *gvahand, gdouble pos, t_GVA_PosUnit pos_unit)
{
  t_GVA_gimp *handle;
  gint32   l_frame_pos;

  handle = (t_GVA_gimp *)gvahand->decoder_handle;

  switch(pos_unit)
  {
    case GVA_UPOS_FRAMES:
      l_frame_pos = (gint32)pos;
      break;
    case GVA_UPOS_SECS:
      l_frame_pos = (gint32)rint (pos * gvahand->framerate);
      break;
    case GVA_UPOS_PRECENTAGE:
      /* is not reliable until all_frames_counted == TRUE */
      l_frame_pos = (gint32)GVA_percent_2_frame(gvahand->total_frames, pos);
      break;
    default:
      l_frame_pos = (gint32)pos;
      break;
  }

  gvahand->percentage_done = 0.0;

  if(l_frame_pos < gvahand->total_frames)
  {
    if(gap_debug)
    {
      printf("p_wrapper_gimp_seek_frame: SEEK OK: handle:%ld l_frame_pos: %d cur_seek:%d cur_frame:%d\n"
          , (long)handle
          , (int)l_frame_pos
          , (int)gvahand->current_seek_nr
          , (int)gvahand->current_frame_nr
          );
    }

    gvahand->current_seek_nr = (gint32)l_frame_pos;

    return(GVA_RET_OK);
  }

  return(GVA_RET_EOF);
}  /* end p_wrapper_gimp_seek_frame */


/* ------------------------------
 * p_wrapper_gimp_seek_audio
 * ------------------------------
 */
t_GVA_RetCode
p_wrapper_gimp_seek_audio(t_GVA_Handle *gvahand, gdouble pos, t_GVA_PosUnit pos_unit)
{
  printf("p_wrapper_gimp_get_audio: there is NO adiosupport for single frames\n");
  return(GVA_RET_ERROR);
}  /* end p_wrapper_gimp_seek_audio */


/* ------------------------------
 * p_wrapper_gimp_get_audio
 * ------------------------------
 */
t_GVA_RetCode
p_wrapper_gimp_get_audio(t_GVA_Handle *gvahand
             ,gint16 *output_i
             ,gint32 channel
             ,gdouble samples
             ,t_GVA_AudPos mode_flag
             )
{
  printf("p_wrapper_gimp_get_audio: there is NO adiosupport for single frames\n");
  return(GVA_RET_ERROR);
}  /* end p_wrapper_gimp_get_audio */


/* ----------------------------------
 * p_wrapper_gimp_count_frames
 * ----------------------------------
 * (re)open a separated handle for counting
 * to ensure that stream positions are not affected by the count.
 */
t_GVA_RetCode
p_wrapper_gimp_count_frames(t_GVA_Handle *gvahand)
{
  gvahand->percentage_done = 0.0;
  /* do not count at all, exact total_frames is known at open_read time */
  return(GVA_RET_OK);
}  /* end p_wrapper_gimp_count_frames */



/* ----------------------------------
 * p_wrapper_gimp_seek_support
 * ----------------------------------
 */
t_GVA_SeekSupport
p_wrapper_gimp_seek_support(t_GVA_Handle *gvahand)
{
  return(GVA_SEEKSUPP_NATIVE);
}  /* end p_wrapper_gimp_seek_support */


/* -----------------------------
 * p_gimp_new_dec_elem
 * -----------------------------
 * create a new decoder element and init with
 * functionpointers referencing the GIMP singleframe
 * specific Procedures
 */
t_GVA_DecoderElem  *
p_gimp_new_dec_elem(void)
{
  t_GVA_DecoderElem  *dec_elem;

  dec_elem = g_malloc0(sizeof(t_GVA_DecoderElem));
  if(dec_elem)
  {
    dec_elem->decoder_name         = g_strdup("gimp/gap");
    dec_elem->decoder_description  = g_strdup("gimp singleframe loader (EXT: .xcf,.jpg,.gif,.tif,.bmp,...)");
    dec_elem->fptr_check_sig       = &p_wrapper_gimp_check_sig;
    dec_elem->fptr_open_read       = &p_wrapper_gimp_open_read;
    dec_elem->fptr_close           = &p_wrapper_gimp_close;
    dec_elem->fptr_get_next_frame  = &p_wrapper_gimp_get_next_frame;
    dec_elem->fptr_seek_frame      = &p_wrapper_gimp_seek_frame;
    dec_elem->fptr_seek_audio      = &p_wrapper_gimp_seek_audio;
    dec_elem->fptr_get_audio       = &p_wrapper_gimp_get_audio;
    dec_elem->fptr_count_frames    = &p_wrapper_gimp_count_frames;
    dec_elem->fptr_seek_support    = &p_wrapper_gimp_seek_support;
    dec_elem->fptr_get_video_chunk = NULL;  /* singleframes dont have compressed video chunks */
    dec_elem->next = NULL;
  }

  return (dec_elem);
}  /* end p_gimp_new_dec_elem */


#endif  /* ENABLE_GVA_GIMP */
