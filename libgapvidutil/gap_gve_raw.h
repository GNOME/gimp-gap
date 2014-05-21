/* gap_gve_raw.h by Wolfgang Hofer (hof@gimp.org)
 *
 * (GAP ... GIMP Animation Plugins, now also known as GIMP Video)
 *    Routines for RAW video encoding
 *    useable for uncompressed AVI video.
 *    (GimpDrawable <--> Raw Bitmap Data)
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
 * version 1.2.2; 2002.12.15  hof: created
 */

#ifndef GAP_GVE_RAW_H
#define GAP_GVE_RAW_H

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

typedef struct GapRgbPixelBuffer
{
  guchar       *data;          /* pointer to region data */
  guint         width;         /* width in pixels */
  guint         height;        /* height in pixels */
  guint         bpp;           /* bytes per pixel (always initialized with 3) */
  guint         rowstride;     /* bytes per pixel row */
} GapRgbPixelBuffer;

/* ------------------------------------
 *  gap_gve_raw_BGR_drawable_encode
 * ------------------------------------
 *  in: drawable: Describes the picture to be compressed in GIMP terms.
 *      vflip: TRUE: flip vertical lines (GIMP has opposite order than uncompressed AVI)
 *      app0_buffer: if != NULL, the content of the APP0-marker to write.
 *      app0_length: the length of the APP0-marker.
 *  out:RAW_size: The size of the buffer that is returned.
 *  returns: guchar *: A buffer, allocated by this routines, which contains
 *                     the uncompressed imagedata B,G,R, NULL on error.
 */
guchar *
gap_gve_raw_BGR_drawable_encode(GimpDrawable *drawable, gint32 *RAW_size, gboolean vflip
                        ,guchar *app0_buffer, gint32 app0_length);


/* ------------------------------------
 * gap_gve_raw_RGB_drawable_encode
 * ------------------------------------
 * Encode drawable to RAW Buffer (Bytesequence RGB)
 * NOTE: this implementation shall be used only in case app0 buffer or vflip feature is required
 *       because it is rather slow due to its row by row processing.
 *       (for faster implementation of RGB conversions see procedure gap_gve_drawable_to_RgbBuffer)
 */
guchar *
gap_gve_raw_RGB_drawable_encode(GimpDrawable *drawable, gint32 *RAW_size, gboolean vflip
                        ,guchar *app0_buffer, gint32 app0_length);

/* ------------------------------------
 * gap_gve_raw_YUV444_drawable_encode
 * ------------------------------------
 * Encode drawable to RAW Buffer (YUV 4:4:4 colormodel)
 * in:
 *    matrix_coefficients 0 upto 6
 *                      0: ITU-R Rec. 709 (1990)
 *                      1: unspecified
 *                      2: reserved
 *                      3: FCC
 *                      4: ITU-R Rec. 624-4 System B, G
 *                      5: SMPTE 170M
 *                      6: SMPTE 240M (1987)
 *
 *  out:RAW_size: The size of the buffer that is returned.
 *  returns: guchar *: A buffer, allocated by this routines, which contains
 *                     the uncompressed imagedata NULL on error.
 *                     for image width =5 , height = 3 the returned buffer is filled
 *                     in the folloing Byte order:
 *                     YYYYY YYYYY YYYYY UUUUU UUUUU UUUUU VVVVV VVVVV VVVVV
 */
guchar *
gap_gve_raw_YUV444_drawable_encode(GimpDrawable *drawable, gint32 *RAW_size, gboolean vflip
                        ,guchar *app0_buffer, gint32 app0_length
                        ,gint32 matrix_coefficients
                        );


/* -----------------------------------
 * gap_gve_raw_YUV420P_drawable_encode
 * -----------------------------------
 * - convert Gimp drawable to YUV420P colormodel
 *   suitable for FFMEG Codec input.
 * - both width and height must be a multiple of 2
 * - output is written to yuv420_buffer (without size checks)
 *   the caller must provide this buffer large enough
 *   (width * height * 1.5)
 *
 *   ouput byte sequence example for image width = 6, height = 4
 *   YYYYYY YYYYYY YYYYYY YYYYYY UUU UUU VVV VVV
 *
 */
void
gap_gve_raw_YUV420P_drawable_encode(GimpDrawable *drawable, guchar *yuv420_buffer);


/* ------------------------------------
 * gap_gve_raw_YUV420_drawable_encode
 * ------------------------------------
 * Encode drawable to RAW Buffer (YUV420 encoded)
 *   suitable for XVID Codec input.
 *
 */
guchar *
gap_gve_raw_YUV420_drawable_encode(GimpDrawable *drawable, gint32 *RAW_size, gboolean vflip
                        ,guchar *app0_buffer, gint32 app0_length);



/* ------------------------------------
 * gap_gve_init_GapRgbPixelBuffer
 * ------------------------------------
 *
 */
void
gap_gve_init_GapRgbPixelBuffer(GapRgbPixelBuffer *rgbBuffer, guint width, guint height);

/* ------------------------------------
 * gap_gve_new_GapRgbPixelBuffer
 * ------------------------------------
 *
 */
GapRgbPixelBuffer *
gap_gve_new_GapRgbPixelBuffer(guint width, guint height);


/* ------------------------------------
 * gap_gve_free_GapRgbPixelBuffer
 * ------------------------------------
 *
 */
void
gap_gve_free_GapRgbPixelBuffer(GapRgbPixelBuffer *rgbBuffer);


/* ------------------------------------
 * gap_gve_drawable_to_RgbBuffer
 * ------------------------------------
 * Encode drawable to RGBBuffer (Bytesequence RGB)
 *
 */
void
gap_gve_drawable_to_RgbBuffer(GimpDrawable *drawable, GapRgbPixelBuffer *rgbBuffer);

/* -----------------------------------------
 * gap_gve_drawable_to_RgbBuffer_multithread
 * -----------------------------------------
 * Encode drawable to RGBBuffer (Bytesequence RGB) on multiprocessor machines.
 * This implementation uses threads to spread the work to the configured
 * number of processors by using a thread pool.
 *
 */
void
gap_gve_drawable_to_RgbBuffer_multithread(GimpDrawable *drawable, GapRgbPixelBuffer *rgbBuffer);



guchar *
gap_gve_raw_RGB_or_BGR_drawable_encode(GimpDrawable *drawable, gint32 *RAW_size, gboolean vflip
                        ,guchar *app0_buffer, gint32 app0_length, gboolean convertToBGR);


#endif

