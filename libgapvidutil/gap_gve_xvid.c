/* gap_gve_xvid.c by Wolfgang Hofer (hof@gimp.org)
 *
 * (GAP ... GIMP Animation Plugins, now also known as GIMP Video)
 *    Routines for MPEG4 (VIDX) video encoding
 *    useable for AVI video.
 *    (GimpDrawable --> MPEG4 Bitmap Data)
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
 * version 1.3.27; 2004.08.02  hof: updated to XVID-1.0 API (but does not work anymore)
 *                                  Colorspace XVID_CSP_RGB24 no longer supported
 * version 1.2.5;  2003.08.02  hof: use Colorspace XVID_CSP_RGB24  gives better quality and fixes Red-Blue colorflip errors
 *                                 (XVID_CSP_YV12 expects strange BGR to YUV encoded buffer
 *                                  gap_gve_raw_YUV420_drawable_encode
 *                                  makes RGB to YUV encoding as needed by FFMPEG)
 * version 1.2.2;  2003.04.18  hof: created
 */

#include <config.h>

/* SYSTEM (UNIX) includes */
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib/gstdio.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_gve_raw.h"
#include "gap_gve_xvid.h"

extern      int gap_debug; /* ==0  ... dont print debug infos */


#ifdef ENABLE_LIBXVIDCORE

/* ------------------------------------
 * p_xvid_debug_write_mp4u_file
 * ------------------------------------
 *
 * create or append encoded buffer to MP4U fileformat
 */

void
p_xvid_debug_write_mp4u_file(guchar *buff, gint32 len)
{
#define LONG_PACK(a,b,c,d) ((long) (((long)(a))<<24) | (((long)(b))<<16) | \
                                   (((long)(c))<<8)  |((long)(d)))

#define SWAP(a) ( (((a)&0x000000ff)<<24) | (((a)&0x0000ff00)<<8) | \
                  (((a)&0x00ff0000)>>8)  | (((a)&0xff000000)>>24) )


  FILE *fp;
  gchar *fname;
  struct stat  l_stat_buf;
  int bigendian;
  long totalsize;

  fname = "/home/hof/tmpvid/out.mp4u";

  totalsize = LONG_PACK('M','P','4','U');
  if(*((char *)(&totalsize)) == 'M')
  {
                bigendian = 1;
  }
  else
  {
                bigendian = 0;
  }

  /* get legth of file */
  if (0 != g_stat(fname, &l_stat_buf))
  {
    long hdr;

    hdr = LONG_PACK('M','P','4','U');

    /* (file does not exist) create it */
    fp = g_fopen(fname, "wb");
    hdr = (!bigendian)?SWAP(hdr):hdr;
    fwrite(&hdr, sizeof(hdr), 1, fp);
  }
  else
  {
    /* (file exists) open append */
    fp = g_fopen(fname, "ab");
  }

  if(fp)
  {
    long size;

    size = len;
    size = (!bigendian)?SWAP(size):size;
    fwrite(&size, sizeof(size), 1, fp);
    fwrite(buff, len, 1, fp);
    fclose (fp);
  }

}  /* end p_xvid_debug_write_mp4u_file */




/*************************************************************
 *          TOOL FUNCTIONS                                   *
 *************************************************************/


void
gap_gve_xvid_algorithm_preset(GapGveXvidValues *xvid_val)
{
  static int const motion_presets[7] = {
        /* quality 0 */
        0,

        /* quality 1 */
        XVID_ME_ADVANCEDDIAMOND16,

        /* quality 2 */
        XVID_ME_ADVANCEDDIAMOND16 | XVID_ME_HALFPELREFINE16,

        /* quality 3 */
        XVID_ME_ADVANCEDDIAMOND16 | XVID_ME_HALFPELREFINE16 |
        XVID_ME_ADVANCEDDIAMOND8 | XVID_ME_HALFPELREFINE8,

        /* quality 4 */
        XVID_ME_ADVANCEDDIAMOND16 | XVID_ME_HALFPELREFINE16 |
        XVID_ME_ADVANCEDDIAMOND8 | XVID_ME_HALFPELREFINE8 |
        XVID_ME_CHROMA_PVOP | XVID_ME_CHROMA_BVOP,

        /* quality 5 */
        XVID_ME_ADVANCEDDIAMOND16 | XVID_ME_HALFPELREFINE16 |
        XVID_ME_ADVANCEDDIAMOND8 | XVID_ME_HALFPELREFINE8 |
        XVID_ME_CHROMA_PVOP | XVID_ME_CHROMA_BVOP,

        /* quality 6 */
        XVID_ME_ADVANCEDDIAMOND16 | XVID_ME_HALFPELREFINE16 | XVID_ME_EXTSEARCH16 |
        XVID_ME_ADVANCEDDIAMOND8 | XVID_ME_HALFPELREFINE8 | XVID_ME_EXTSEARCH8 |
        XVID_ME_CHROMA_PVOP | XVID_ME_CHROMA_BVOP,

  };

  static const int vop_presets[] = {
        /* quality 0 */
        0,

        /* quality 1 */
        0,

        /* quality 2 */
        XVID_VOP_HALFPEL,

        /* quality 3 */
        XVID_VOP_HALFPEL | XVID_VOP_INTER4V,

        /* quality 4 */
        XVID_VOP_HALFPEL | XVID_VOP_INTER4V,

        /* quality 5 */
        XVID_VOP_HALFPEL | XVID_VOP_INTER4V |
        XVID_VOP_TRELLISQUANT,

        /* quality 6 */
        XVID_VOP_HALFPEL | XVID_VOP_INTER4V |
        XVID_VOP_TRELLISQUANT | XVID_VOP_HQACPRED,

  };
  if((xvid_val->quality_preset >= 0) && (xvid_val->quality_preset < 7))
  {
    xvid_val->general = vop_presets[xvid_val->quality_preset];
    xvid_val->motion  = motion_presets[xvid_val->quality_preset];
  }
}


/* ------------------------------------
 * gap_gve_xvid_init
 * ------------------------------------
 * init XVID and create the encoder instance
 *
 */
GapGveXvidControl *
gap_gve_xvid_init(gint32 width, gint32 height, gdouble framerate, GapGveXvidValues *xvid_val)
{
  GapGveXvidControl     *xvid_control;
  xvid_gbl_init_t    *xvid_gbl_init;
  xvid_enc_create_t  *xvid_enc_create;
  xvid_enc_frame_t   *xvid_enc_frame;
  int xerr;
  gdouble         l_framerate_x1000;

  l_framerate_x1000 = framerate * 1000.0;
  xvid_control = g_malloc0(sizeof(GapGveXvidControl));
  xvid_gbl_init   = &xvid_control->xvid_gbl_init;
  xvid_enc_create = &xvid_control->xvid_enc_create;
  xvid_enc_frame  = &xvid_control->xvid_enc_frame;

  /* ------------------------
   * XviD core initialization
   * -------------------------
   */
  memset(xvid_gbl_init, 0, sizeof(xvid_gbl_init_t));
  xvid_gbl_init->version = XVID_VERSION;
  xvid_gbl_init->debug = 0;
  xvid_gbl_init->cpu_flags = 0;
  xvid_gbl_init->cpu_flags = XVID_CPU_FORCE;

  if(gap_debug)
  {
    printf("gap_gve_xvid_init XVID_VERSION: %d (%d.%d.%d)\n"
          ,(int)xvid_gbl_init->version
          ,(int)XVID_VERSION_MAJOR(xvid_gbl_init->version)
          ,(int)XVID_VERSION_MINOR(xvid_gbl_init->version)
          ,(int)XVID_VERSION_PATCH(xvid_gbl_init->version)
          );
  }
    

  /* Initialize XviD core -- Should be done once per __process__ */
  xvid_global(NULL, XVID_GBL_INIT, xvid_gbl_init, NULL);

  /* ------------------------
   * XviD encoder initialization
   * -------------------------
   */
  /* Version again */
  memset(xvid_enc_create, 0, sizeof(xvid_enc_create_t));
  xvid_enc_create->version = XVID_VERSION;


  /* Width and Height of input frames */
  xvid_enc_create->width                    = width;
  xvid_enc_create->height                   = height;
  xvid_enc_create->profile                  = XVID_PROFILE_AS_L4;

  xvid_enc_create->fincr                    = 1000;
  xvid_enc_create->fbase                    = l_framerate_x1000;

  /* init zones  */
  xvid_control->num_zones = 0;
  if(FALSE)
  {
    /* the current implementation does not use zones
     * but here is an example what to initialize per zone
     * for later use
     */
    if(TRUE)
    {
      xvid_control->zones[xvid_control->num_zones].mode = XVID_ZONE_QUANT;
    }
    else
    {
      xvid_control->zones[xvid_control->num_zones].mode = XVID_ZONE_WEIGHT;
    }
    xvid_control->zones[xvid_control->num_zones].frame = 0;
    xvid_control->zones[xvid_control->num_zones].increment = 100 * 1;
    xvid_control->zones[xvid_control->num_zones].base = 100;
    xvid_control->num_zones++;
  }
  xvid_enc_create->zones = &xvid_control->zones[0];
  xvid_enc_create->num_zones = xvid_control->num_zones;

  /* init plugins  */
  xvid_enc_create->plugins = xvid_control->plugins;
  xvid_enc_create->num_plugins = 0;

  /* this implementation only uses SINGLE PASS */
  {
    memset(&xvid_control->single, 0, sizeof(xvid_plugin_single_t));
    xvid_control->single.version = XVID_VERSION;
    xvid_control->single.bitrate = xvid_val->rc_bitrate;

    xvid_control->plugins[xvid_enc_create->num_plugins].func = xvid_plugin_single;
    xvid_control->plugins[xvid_enc_create->num_plugins].param = &xvid_control->single;
    xvid_enc_create->num_plugins++;
  }

  /* TODO: lumimasking should be a Parameter */
  if(FALSE)
  {
    xvid_control->plugins[xvid_enc_create->num_plugins].func = xvid_plugin_lumimasking;
    xvid_control->plugins[xvid_enc_create->num_plugins].param = NULL;
    xvid_enc_create->num_plugins++;
  }

  /* TODO: Dump should be a Parameter */
  if(FALSE)
  {
    xvid_control->plugins[xvid_enc_create->num_plugins].func = xvid_plugin_dump;
    xvid_control->plugins[xvid_enc_create->num_plugins].param = NULL;
    xvid_enc_create->num_plugins++;
  }

  /* parameters of older xvid version (dont know how to migrate to v1.0) */
  // xvid_encparam->rc_reaction_delay_factor = xvid_val->rc_reaction_delay_factor;
  // xvid_encparam->rc_averaging_period      = xvid_val->rc_averaging_period;
  // xvid_encparam->rc_buffer                = xvid_val->rc_buffer;
  // xvid_encparam->max_quantizer            = xvid_val->max_quantizer;
  // xvid_encparam->min_quantizer            = xvid_val->min_quantizer;

  /* Maximum key frame interval */
  xvid_enc_create->max_key_interval          = xvid_val->max_key_interval;

  /* No fancy thread tests */
  xvid_enc_create->num_threads = 0;

  /* Bframes settings TODO: pass params for Bframe settings  */
  xvid_enc_create->max_bframes = 0;                // ARG_MAXBFRAMES;
  xvid_enc_create->bquant_ratio = 150;             // ARG_BQRATIO;
  xvid_enc_create->bquant_offset = 100;            // ARG_BQOFFSET;

  /* Dropping ratio frame -- we don't need that */
  xvid_enc_create->frame_drop_ratio = 0;

  /* Global encoder options TODO: pass params for Global options */
  xvid_enc_create->global = 0;
  if (FALSE)
  {
    xvid_enc_create->global |= XVID_GLOBAL_PACKED;
  }

  if (FALSE)
  {
    xvid_enc_create->global |= XVID_GLOBAL_CLOSED_GOP;
  }

  if (FALSE)
  {
    xvid_enc_create->global |= XVID_GLOBAL_EXTRASTATS_ENABLE;
  }


  xvid_enc_create->handle =  NULL;      /* out param The encoder instance */

  xvid_enc_frame->version            = XVID_VERSION;
  xvid_enc_frame->length             = -1;      /* out: length returned by encoder */
  xvid_enc_frame->input.csp          = XVID_CSP_YV12;  /*XVID_CSP_I420  XVID_CSP_BGR  | XVID_CSP_VFLIP */


  /* Set up core's general features */
  xvid_enc_frame->vol_flags = 0;
  if (FALSE)
  {
    xvid_enc_frame->vol_flags |= XVID_VOL_EXTRASTATS;
  }
  if (FALSE)
  {
    xvid_enc_frame->vol_flags |= XVID_VOL_QUARTERPEL;
  }
  if (FALSE)
  {
    xvid_enc_frame->vol_flags |= XVID_VOL_GMC;
  }


  xvid_enc_frame->vop_flags = xvid_val->general;
  xvid_enc_frame->motion    = xvid_val->motion;

  if(FALSE)
  {
    xvid_enc_frame->motion |= XVID_ME_GME_REFINE;
  }
  if(FALSE)
  {
    xvid_enc_frame->motion |= XVID_ME_QUARTERPELREFINE16;
  }
  if(FALSE && (xvid_enc_frame->vop_flags & XVID_VOP_INTER4V))
  {
    xvid_enc_frame->motion |= XVID_ME_QUARTERPELREFINE8;
  }
  xvid_enc_frame->quant_intra_matrix = NULL;   /* use built in default Matrix */
  xvid_enc_frame->quant_inter_matrix = NULL;   /* use built in default Matrix */
  xvid_enc_frame->quant              = 0;      /* 0: codec decides, 1..31 force quant for this frame */
  xvid_enc_frame->type = XVID_TYPE_AUTO;       /* Frame type -- let core decide for us */


  /* create the encoder instance */
  xerr = xvid_encore(NULL, XVID_ENC_CREATE, xvid_enc_create, NULL);

  if(xerr)
  {
    printf("gap_gve_xvid_init create encoder instance failed. ERRORCODE: %d\n", (int)xerr);
    g_free(xvid_control);
    return (NULL);
  }

  return(xvid_control);
}   /* end gap_gve_xvid_init */


/* ------------------------------------
 * gap_gve_xvid_drawable_encode
 * ------------------------------------
 * Encode drawable to Buffer with xvid (Open DivX) encoding
 *
 */
guchar *
gap_gve_xvid_drawable_encode(GimpDrawable *drawable, gint32 *XVID_size,  GapGveXvidControl  *xvid_control
                      , int *keyframe
                      , void *app0_buffer, gint32 app0_length)
{
  gint32               RAW_size;
  guchar              *bitstream_data;
  guchar              *bitstream_ptr;
  gboolean             l_vflip;

  xvid_enc_frame_t *xvid_enc_frame;
  xvid_enc_stats_t xvid_enc_stats;
  int xerr;

  memset(&xvid_enc_stats, 0, sizeof(xvid_enc_stats));
  xvid_enc_stats.version = XVID_VERSION;


  /* buffer for encoding (allocate in full uncompressed size to be on the save side) */
  bitstream_data = (guchar *)g_malloc0((drawable->width * drawable->height * 3)
                                       + app0_length);
  bitstream_ptr = bitstream_data;
  if(app0_buffer)
  {
    memcpy(bitstream_data, app0_buffer, app0_length);
    bitstream_ptr += app0_length;
  }

  xvid_enc_frame = &xvid_control->xvid_enc_frame;

  xvid_enc_frame->bitstream          = bitstream_ptr;
  xvid_enc_frame->length             = -1;      /* out: length returned by encoder */
  /*xvid_enc_frame->input.csp         = XVID_CSP_YV12;*/  /*XVID_CSP_I420 XVID_CSP_BGR  | XVID_CSP_VFLIP */
  xvid_enc_frame->input.csp          = XVID_CSP_BGR;  /*XVID_CSP_I420  XVID_CSP_BGR | XVID_CSP_VFLIP */
  xvid_enc_frame->quant_intra_matrix = NULL;   /* use built in default Matrix */
  xvid_enc_frame->quant_inter_matrix = NULL;   /* use built in default Matrix */
  xvid_enc_frame->quant              = 0;      /* 0: codec decides, 1..31 force quant for this frame */
  xvid_enc_frame->type               = XVID_TYPE_AUTO;     /* In/Out: let the codec decide between I-frame (1) and P-frame (0) */

  /* XVID colorspace XVID_CSP_BGR uses  b,g,r packed 8bit per pixel (?? or r,g,b order ??)
   * XVID colorspace I420 colorsapce (y,u,v planar)
   *
   * (we use the raw encoding procedure to convert the drawable to BGR
   *  suitable for xvid input)
   * (do?) we use vertical flip (yes for older xvid, no since 1.0.0)
   * (RAW AVI and gimp have opposite vertical order of the pixel lines)
   */
  l_vflip = FALSE;

  /* XVID_CSP_BGR */
  {
    //xvid_enc_frame->input.plane[0] = gap_gve_raw_RGB_drawable_encode(drawable, &RAW_size, l_vflip, NULL, 0);
    xvid_enc_frame->input.plane[0] = gap_gve_raw_BGR_drawable_encode(drawable, &RAW_size, l_vflip, NULL, 0);
    xvid_enc_frame->input.stride [0] = (3 * xvid_control->xvid_enc_create.width);
  }
//  /* XVID_CSP_YV12 */
//  {
//    xvid_enc_frame->input.plane[0] = gap_gve_raw_YUV420_drawable_encode(drawable, &RAW_size, l_vflip, NULL, 0);
//    xvid_enc_frame->input.stride [0] = ???;
//  }

  if(xvid_enc_frame->input.plane[0])
  {
    /* This Call to xvid_encore Encodes one Frame with xvid compression */
    xerr = xvid_encore(xvid_control->xvid_enc_create.handle, XVID_ENC_ENCODE, xvid_enc_frame, &xvid_enc_stats);
    g_free(xvid_enc_frame->input.plane[0]);

    if(gap_debug)
    {
      printf("gap_gve_xvid_drawable_encode returnCODE: %d\n", (int)xerr);
      printf("gap_gve_xvid_drawable_encode xvid_enc_frame->length: %d\n", (int)xvid_enc_frame->length);
    }
    
    /* since xvid 1.0 the encoded length  is returned,
     * and errorcodes are negative values,
     * the xvid_enc_frame->length field is ignored now
     */
    if(xerr>=0)
    {
      *keyframe = xvid_enc_frame->type;
      /*  *XVID_size = xvid_enc_frame->length + app0_length; */
      *XVID_size = xerr + app0_length;

      /*if(1==1)
       *{
       *   p_xvid_debug_write_mp4u_file(bitstream_data, *XVID_size);
       *}
       */

      return(bitstream_data);
    }
    printf("gap_gve_xvid_drawable_encode returned ERRORCODE: %d\n", (int)xerr);
  }
  else
  {
    printf("gap_gve_xvid_drawable_encode ERROR gap_gve_raw_RGB_drawable_encode FAILED!\n");
  }

  *keyframe = FALSE;
  *XVID_size = 0;
  g_free(bitstream_data);
  return (NULL);

} /* end gap_gve_xvid_drawable_encode */


/* ------------------------------------
 * gap_gve_xvid_cleanup
 * ------------------------------------
 * destroy XVID encoder instance
 *
 */
void
gap_gve_xvid_cleanup(GapGveXvidControl  *xvid_control)
{
  xvid_encore(xvid_control->xvid_enc_create.handle, XVID_ENC_DESTROY, NULL, NULL);
}

#endif /* ENABLE_LIBXVIDCORE */
