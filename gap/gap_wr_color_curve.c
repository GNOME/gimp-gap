/* gap_wr_color_curve.c
 * 2000.Feb.21 hof (Wolfgang Hofer)
 *
 *  Wrapper Plugin for GIMP Curves tool
 *
 * This HACK enables
 * Animated Filterapply in conjunction with the
 * GIMP Curve Tool.
 *
 *  It provides a primitive Dialog Interface where you
 *  can load Curves (from Files saved by the Curve Tool of GIMP releases 1.2 up to GIMP-2.6)
 *  and run The Curve Tool with these values as a Plugin on the current Drawable.
 *
 *  Further it has an Interface to 'Run_with_last_values'
 *  and an Iterator Procedure.
 *  (This enables the 'Animated Filter Call' from
 *   the GAP's Menu Filters->Filter all Layers
 *   and You can create Curves based Coloranimations)
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

/* Revision history
 *  (2005/01/29)  v2.1   hof: added Help
 *  (2004/01/15)  v1.3   hof: integrated into GIMP-GAP
 *  (2003/10/30)  v1.3   hof: adapted for gimp-1.3.x and gtk+2.2 API
 *  (2002/10/27)  v1.03  hof: appear in menu (enable filtermacro use)
 *  (2002/01/01)  v1.02  hof: removed GIMP_ENABLE_COMPAT_CRUFT (gimp 1.2.x)
 *  (2000/10/06)  v1.01  hof: - GIMP_ENABLE_COMPAT_CRUFT to compile with gimptool 1.1.26
 *  (2000/02/22)  v1.0   hof: first public release
 *  (2000/02/21)  v0.0   hof: coding started,
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <glib/gstdio.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_libgapbase.h"
#include "gap_stock.h"

#include "gap-intl.h"


/* Defines */
#define PLUG_IN_NAME        "plug-in-wr-curves"
#define PLUG_IN_IMAGE_TYPES "RGB*, GRAY*"
#define PLUG_IN_AUTHOR      "Wolfgang Hofer (hof@gimp.org)"
#define PLUG_IN_COPYRIGHT   "Wolfgang Hofer"
#define PLUG_IN_DESCRIPTION "Wrapper for GIMP Curves Tool call based on Curves file"

#define PLUG_IN_ITER_NAME       "plug-in-wr-curves-Iterator"
#define PLUG_IN_DATA_ITER_FROM  "plug-in-wr-curves-ITER-FROM"
#define PLUG_IN_DATA_ITER_TO    "plug-in-wr-curves-ITER-TO"

#define PLUG_IN_HELP_ID         "plug-in-wr-curves"

/* pointval_t
 *  wanted to use gdouble to store plugin data
 *  but in older version of gap_filter_pdb.h
 *  there is a hardcoded limt
 *
 *    PLUGIN_DATA_SIZE       8192
 *
 *  pointval_t guint8  precision will do it for the
 *  curves tool.
 *  But you may set pointval_t to gdouble
 *  if you have a gimp/gap version newer than 1.1.17
 */

typedef guint8 pointval_t;
/* typedef gdouble pointval_t; */

typedef struct
{
  pointval_t val_curve[5][256];
} wr_curves_val_t;


/* The Stuff for calculating the Curve
 * was taken from gimp-1.1.17/app/curves.c
 * (and reduced for non-ineracive usage)
 */
#define SMOOTH 0
#define GFREE  1

typedef double CRMatrix[4][4];

typedef struct _CurvesDialog CurvesDialog;

struct _CurvesDialog
{
  gint          channel;

  gint          curve_type[5];
  gint          points[5][17][2];
  guchar        curve[5][256];
  gint          col_value[5];

};

typedef struct _WrCurveDialog WrCurveDialog;

struct _WrCurveDialog
{
  gint          run;
  gint          show_progress;
  gchar        *filename;
  GtkWidget *shell;
  GtkWidget *filesel;
  GtkWidget *entry;

};


/* stuff to parse curves format introduced with GIMP-2.6 */

#define TOKEN_SAMPLES "samples"
#define TOKEN_CHANNEL "channel"
#define TOKEN_CURVE   "curve"
#define MAX_CHANNELS 5
#define MAX_FILESIZE 800000

typedef struct { /* nick cpc */
  gchar *buffer;                  /* holds the complete file content without the header line */
  gchar *ptr;                     /* (Do not g_free this) pointer to current parsing position in the buffer */
  gint channel_index;             /* curent channel */
  gint point_index;               /* current point in the current channel */ 
  gboolean channel_found[MAX_CHANNELS];
  gboolean samples_found[MAX_CHANNELS];
  wr_curves_val_t *cuvals;        /* (Do not g_free this) reference */
  
} CurveParserContext;



/* ============= stuff for curves format of GIMP-1.1 up to GIMP-2.4 release */
static void   curves_CR_compose      (CRMatrix, CRMatrix, CRMatrix);
static void   curves_plot_curve      (CurvesDialog *, int, int, int, int);
static void   curves_calculate_curve (CurvesDialog *cd);

static gboolean  read_curves_from_file (const char *filename, FILE *fp, wr_curves_val_t *cuvals);

/* ============== stuff for curves format of GIMP-2.6 release */
static CurveParserContext*    p_new_CurveParserContext(gint32 fileSize, FILE *fp, wr_curves_val_t *cuvals);
static gint32                 p_read_gint32_value(CurveParserContext *cpc);
static gdouble                p_read_gdouble_value(CurveParserContext *cpc);
static void                   p_skip_whitespace(CurveParserContext *cpc);
static void                   p_skip_until_opening_bracket(CurveParserContext *cpc);
static void                   p_skip_until_closing_bracket(CurveParserContext *cpc);
static void                   p_read_channel(CurveParserContext *cpc);
static void                   p_read_samples(CurveParserContext *cpc);
static void                   p_read_curve(CurveParserContext *cpc);

static gboolean  p_read_curves_from_file_gimp2_6_format (const char *filename, FILE *fp, wr_curves_val_t *cuvals, gchar *buf);



static int       p_load_curve(gchar *filename, wr_curves_val_t *cuvals);
static void      p_delta_pointval_t (pointval_t  *val,
                    pointval_t   val_from,
                    pointval_t   val_to,
                    gint32   total_steps,
                    gdouble  current_step);

static gint32    p_gimp_curves_explicit (gint32 drawable_id,
                                       gint32 channel,
                                       gint32 num_bytes,
                                       gint8 *curve_points);
static void      p_run_curves_tool(gint32 drawable_id, wr_curves_val_t *cuvals);


/* stuff for the dialog */
static void      p_filesel_close_cb (GtkWidget *widget, gpointer   data);
static void      p_filesel_ok_callback (GtkWidget *widget, gpointer   data);
static void      wr_curve_load_callback (GtkWidget *w,  WrCurveDialog *wcd);
static void      text_entry_callback(GtkWidget *widget, WrCurveDialog *wcd);
static void      wr_curve_response (GtkWidget *widget, gint response_id, WrCurveDialog *gpp);
WrCurveDialog   *do_dialog (wr_curves_val_t *);


/* required stuff for gimp-plug-ins */
static void  query (void);
static void  run(const gchar *name
           , gint nparams
           , const GimpParam *param
           , gint *nreturn_vals
           , GimpParam **return_vals);

static gboolean  p_read_curves_from_file_gimp2_6_format (const char *filename, FILE *fp, wr_curves_val_t *cuvals, gchar *buf);



/* Global Variables */
GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,   /* init_proc  */
  NULL,   /* quit_proc  */
  query,  /* query_proc */
  run     /* run_proc   */
};

static CRMatrix CR_basis =
{
  { -0.5,  1.5, -1.5,  0.5 },
  {  1.0, -2.5,  2.0, -0.5 },
  { -0.5,  0.0,  0.5,  0.0 },
  {  0.0,  1.0,  0.0,  0.0 },
};

/* Global Variables */
int gap_debug = 0;  /* 1 == print debug infos , 0 dont print debug infos */

/* --------------
 * procedures
 * --------------
 */

/* ============================ START curve calculation from GIMP-2.4 curves fileformat ====================== */

static void
curves_CR_compose (CRMatrix a,
                   CRMatrix b,
                   CRMatrix ab)
{
  gint i, j;

  for (i = 0; i < 4; i++)
    {
      for (j = 0; j < 4; j++)
        {
          ab[i][j] = (a[i][0] * b[0][j] +
                      a[i][1] * b[1][j] +
                      a[i][2] * b[2][j] +
                      a[i][3] * b[3][j]);
        }
    }
}


static void
curves_plot_curve (CurvesDialog *cd,
                   gint          p1,
                   gint          p2,
                   gint          p3,
                   gint          p4)
{
  CRMatrix geometry;
  CRMatrix tmp1, tmp2;
  CRMatrix deltas;
  double x, dx, dx2, dx3;
  double y, dy, dy2, dy3;
  double d, d2, d3;
  int lastx, lasty;
  gint32 newx, newy;
  int i;

  /* construct the geometry matrix from the segment */
  for (i = 0; i < 4; i++)
    {
      geometry[i][2] = 0;
      geometry[i][3] = 0;
    }

  for (i = 0; i < 2; i++)
    {
      geometry[0][i] = cd->points[cd->channel][p1][i];
      geometry[1][i] = cd->points[cd->channel][p2][i];
      geometry[2][i] = cd->points[cd->channel][p3][i];
      geometry[3][i] = cd->points[cd->channel][p4][i];
    }

  /* subdivide the curve 1000 times */
  /* n can be adjusted to give a finer or coarser curve */
  d = 1.0 / 1000;
  d2 = d * d;
  d3 = d * d * d;

  /* construct a temporary matrix for determining the forward differencing deltas */
  tmp2[0][0] = 0;     tmp2[0][1] = 0;     tmp2[0][2] = 0;    tmp2[0][3] = 1;
  tmp2[1][0] = d3;    tmp2[1][1] = d2;    tmp2[1][2] = d;    tmp2[1][3] = 0;
  tmp2[2][0] = 6*d3;  tmp2[2][1] = 2*d2;  tmp2[2][2] = 0;    tmp2[2][3] = 0;
  tmp2[3][0] = 6*d3;  tmp2[3][1] = 0;     tmp2[3][2] = 0;    tmp2[3][3] = 0;

  /* compose the basis and geometry matrices */
  curves_CR_compose (CR_basis, geometry, tmp1);

  /* compose the above results to get the deltas matrix */
  curves_CR_compose (tmp2, tmp1, deltas);

  /* extract the x deltas */
  x = deltas[0][0];
  dx = deltas[1][0];
  dx2 = deltas[2][0];
  dx3 = deltas[3][0];

  /* extract the y deltas */
  y = deltas[0][1];
  dy = deltas[1][1];
  dy2 = deltas[2][1];
  dy3 = deltas[3][1];

  lastx = CLAMP (x, 0, 255);
  lasty = CLAMP (y, 0, 255);

  cd->curve[cd->channel][lastx] = lasty;

  /* loop over the curve */
  for (i = 0; i < 1000; i++)
    {
      /* increment the x values */
      x += dx;
      dx += dx2;
      dx2 += dx3;

      /* increment the y values */
      y += dy;
      dy += dy2;
      dy2 += dy3;

      newx = CLAMP0255 (ROUND (x));
      newy = CLAMP0255 (ROUND (y));

      /* if this point is different than the last one...then draw it */
      if ((lastx != newx) || (lasty != newy))
        cd->curve[cd->channel][newx] = newy;

      lastx = newx;
      lasty = newy;
    }
}

static void
curves_calculate_curve (CurvesDialog *cd)
{
  int i;
  int points[17];
  int num_pts;
  int p1, p2, p3, p4;

  switch (cd->curve_type[cd->channel])
    {
    case GFREE:
      break;
    case SMOOTH:
      /*  cycle through the curves  */
      num_pts = 0;
      for (i = 0; i < 17; i++)
        if (cd->points[cd->channel][i][0] != -1)
          points[num_pts++] = i;

      /*  Initialize boundary curve points */
      if (num_pts != 0)
        {
          for (i = 0; i < cd->points[cd->channel][points[0]][0]; i++)
            cd->curve[cd->channel][i] = cd->points[cd->channel][points[0]][1];
          for (i = cd->points[cd->channel][points[num_pts - 1]][0]; i < 256; i++)
            cd->curve[cd->channel][i] = cd->points[cd->channel][points[num_pts - 1]][1];
        }

      for (i = 0; i < num_pts - 1; i++)
        {
          p1 = (i == 0) ? points[i] : points[(i - 1)];
          p2 = points[i];
          p3 = points[(i + 1)];
          p4 = (i == (num_pts - 2)) ? points[(num_pts - 1)] : points[(i + 2)];

          curves_plot_curve (cd, p1, p2, p3, p4);
        }
      break;
    }

}


/* ----------------------------------------
 * read_curves_from_file
 * ----------------------------------------
 */
static gboolean
read_curves_from_file (const char *filename, FILE *fp, wr_curves_val_t *cuvals)
{
  gint i, j, fields;
  gchar buf[50];
  gint index[5][17];
  gint value[5][17];
  gint current_channel;
  CurvesDialog   curves_dialog_struct;
  CurvesDialog   *curves_dialog = &curves_dialog_struct;

  if (!fgets (buf, 50, fp))
  {
    return FALSE;
  }
  
  /* check old format used in GIMP-2.4.x and older GIMP releases */
  if (strcmp (buf, "# GIMP Curves File\n") != 0)
  {
    /* check new format introduced with GIMP-2.6.x release */
    return (p_read_curves_from_file_gimp2_6_format (filename, fp, cuvals, buf));
  }
  
  for (i = 0; i < 5; i++)
  {
    for (j = 0; j < 17; j++)
        {
          fields = fscanf (fp, "%d %d ", &index[i][j], &value[i][j]);
          if (fields != 2)
            {
              g_print ("fields != 2");
              return FALSE;
            }
        }
  }

  for (i = 0; i < 5; i++)
  {
    curves_dialog->curve_type[i] = SMOOTH;
    for (j = 0; j < 17; j++)
        {
          curves_dialog->points[i][j][0] = index[i][j];
          curves_dialog->points[i][j][1] = value[i][j];
        }
  }

  /* this is ugly, but works ... */
  current_channel = curves_dialog->channel;
  for (i = 0; i < 5; i++)
  {
      curves_dialog->channel = i;
      curves_calculate_curve (curves_dialog);
      for(j = 0; j < 256; j++)
      {
            cuvals->val_curve[i][j] = (pointval_t)curves_dialog->curve[i][j];
      }
  }
  curves_dialog->channel = current_channel;

  return TRUE;
}


/* ============================ START parsing GIMP-2.6 curves file ====================== */
 



/* ----------------------------------------
 * p_new_CurveParserContext
 * ----------------------------------------
 * loads the Curve settings from the specified file,
 * (starting at current position, (typically full content that follows the header line)
 * and sts up a context for parsing.
 */
static CurveParserContext*
p_new_CurveParserContext(gint32 fileSize, FILE *fp, wr_curves_val_t *cuvals)
{
  CurveParserContext *cpc;
  gint ii;
  
  cpc = g_new(CurveParserContext, 1);
  
  cpc->buffer = (gchar *) g_malloc0(fileSize+1);

  /* read all the rest of the file into buffer */
  fread(cpc->buffer, 1, (size_t)fileSize, fp);
  
  cpc->ptr = cpc->buffer;
  cpc->channel_index = 0;
  cpc->point_index = 0;
  cpc->cuvals = cuvals;
  for(ii=0; ii < MAX_CHANNELS; ii++)
  {
    cpc->channel_found[ii] = FALSE;
    cpc->samples_found[ii] = FALSE;
  }
  
  return (cpc);
  
}  /* end p_new_CurveParserContext */


/* ----------------------------------------
 * p_read_gint32_value
 * ----------------------------------------
 * read integer value and advance scan ptr to char after the number
 */
static gint32
p_read_gint32_value(CurveParserContext *cpc)
{
  gchar  *l_end_ptr;
  long   l_num;

  l_num = 0;
  if(cpc->ptr)
  {
    if(*cpc->ptr != '\0')
    {
      l_end_ptr = cpc->ptr;
      l_num = strtol(cpc->ptr, &l_end_ptr, 10);
      if (cpc->ptr != l_end_ptr)
      {
         cpc->ptr = l_end_ptr;
         return ((gint32)l_num);
      }
      printf("ERROR incompatible Curve settings, scan of int value failed\n");
    }
  }
  return (l_num);
}  /* end p_read_gint32_value */


/* ----------------------------------------
 * p_read_gdouble_value
 * ----------------------------------------
 * read doble value and advance scan ptr to char after the number
 */
static gdouble 
p_read_gdouble_value(CurveParserContext *cpc)
{
  char   *l_end_ptr;
  double  l_doubleValue;

  l_doubleValue = 0.0;
  if(cpc->ptr)
  {
    if(*cpc->ptr != '\0')
    {
      l_end_ptr = cpc->ptr;
      l_doubleValue = g_ascii_strtod(cpc->ptr, &l_end_ptr);
      if (cpc->ptr != l_end_ptr)
      {
         cpc->ptr = l_end_ptr;
         return ((gdouble)l_doubleValue);
      }
      printf("ERROR incompatible Curve settings, scan of double value failed\n");
    }

  }
  return (l_doubleValue);
}  /* end p_read_gdouble_value */


/* ----------------------------------------
 * p_skip_whitespace
 * ----------------------------------------
 */
static void
p_skip_whitespace(CurveParserContext *cpc)
{
  while(*cpc->ptr != '\0')
  {
    switch (*cpc->ptr)
    {
      case ' ':
      case '\t':
      case '\r':
      case '\n':
        break;
      case '#':
        while (*cpc->ptr != '\n')
        {
          cpc->ptr++;
          if (*cpc->ptr == '\0')
          {
            return;
          }
        }
        break;
      default:
        return;
        break;
    }
    cpc->ptr++;
  }
}  /* end p_skip_whitespace */


/* ----------------------------------------
 * p_skip_until_opening_bracket
 * ----------------------------------------
 * advance scan ptr to position after the next opening bracket
 */
static void
p_skip_until_opening_bracket(CurveParserContext *cpc)
{
  while(*cpc->ptr != '\0')
  {
    if (*cpc->ptr == '(')
    {
      cpc->ptr++;
      return;
    }
    cpc->ptr++;
  }
}  /* end p_skip_until_opening_bracket */


/* ----------------------------------------
 * p_skip_until_closing_bracket
 * ----------------------------------------
 * advance scan ptr to position after the closing bracket
 */
static void
p_skip_until_closing_bracket(CurveParserContext *cpc)
{
  int countBr;
  
  countBr = 0;
  
  while(*cpc->ptr != '\0')
  {
    switch (*cpc->ptr)
    {
      case '(':
        countBr++;
        break;
      case ')':
        countBr--;
        if (countBr < 0)
        {
          cpc->ptr++;
          return;
        }
        break;
    }
    cpc->ptr++;
  }
  
}  /* end p_skip_until_closing_bracket */


/* ----------------------------------------
 * p_read_channel
 * ----------------------------------------
 */
static void
p_read_channel(CurveParserContext *cpc)
{
  static const char *channel_name[] = {"value", "red", "green", "blue", "alpha" };
  gint ii;
  
  cpc->ptr += strlen(TOKEN_CHANNEL);
  p_skip_whitespace(cpc);
  
  for (ii = 0; ii < MAX_CHANNELS; ii++)
  {
    int lenChannelName;
    
    lenChannelName = strlen(channel_name[ii]);
    if(strncmp(cpc->ptr, channel_name[ii], lenChannelName) == 0)
    {
      cpc->channel_index = ii;
      cpc->channel_found[ii] = TRUE;
      if(gap_debug)
      {
        printf("\nCHANNEL token %s index:%d\n", channel_name[ii], cpc->channel_index );
      }
    }
  }

  p_skip_until_closing_bracket(cpc);

}  /* end p_read_channel */


/* ----------------------------------------
 * p_read_samples
 * ----------------------------------------
 * read the samples token followed by one integer and 256 double values
 * set scan position after the closing bracket of the samples token.
 */
static void
p_read_samples(CurveParserContext *cpc)
{
  gint32 intValue;
  gdouble  doubleValue;
  
  cpc->ptr += strlen(TOKEN_SAMPLES);
  p_skip_whitespace(cpc);

  intValue = p_read_gint32_value(cpc);
  if(gap_debug)
  {
    printf("p_read_samples START intValue:%d\n", (int)intValue);
  }
  cpc->point_index = 0;
  while(*cpc->ptr != '\0')
  {
    p_skip_whitespace(cpc);
    doubleValue = p_read_gdouble_value(cpc);
    cpc->cuvals->val_curve[cpc->channel_index][cpc->point_index] = CLAMP0255 (ROUND (doubleValue * 255));
    
    if(gap_debug)
    {
      printf(" [%d] [%03d] %d  %f\n"
        , (int) cpc->channel_index
        , (int) cpc->point_index
        , (int) cpc->cuvals->val_curve[cpc->channel_index][cpc->point_index]
        , (float) doubleValue
        );
    }
    
    
    cpc->point_index++;
    
    /* check if all sample points done */
    if (cpc->point_index >= 256)
    {
      cpc->samples_found[cpc->channel_index] = TRUE;
      p_skip_until_closing_bracket(cpc);
      return;
    }

    if (*cpc->ptr == ')')
    {
      cpc->ptr++;
      return;
    }
  }

}  /* end p_read_samples */


/* ----------------------------------------
 * p_read_curve
 * ----------------------------------------
 * read curve description 
 *  this implementation read only the expected 256 samples and ignores (i.e. skips) other parts
 *  of the curve description
 * the scan ptr is set to the character after the closing bracket.
 */
static void
p_read_curve(CurveParserContext *cpc)
{
  cpc->ptr += strlen(TOKEN_CURVE);

  while(*cpc->ptr != '\0')
  {
    p_skip_whitespace(cpc);
    p_skip_until_opening_bracket(cpc);

    if(gap_debug)
    {
      printf("\n\np_read_curve cpc->ptr:%100.100s\n", cpc->ptr);
    }

    if(strncmp(cpc->ptr, TOKEN_SAMPLES, strlen(TOKEN_SAMPLES)) == 0)
    {
       p_read_samples(cpc);
    } 
    else
    {
       /* ignore other tokens within the curve description */
       p_skip_until_closing_bracket(cpc);
    }
    
    /* check if closing bracket of the curves description is reached */
    if (*cpc->ptr == ')')
    {
      cpc->ptr++;
      return;
    }
  }

}  /* end p_read_curve */


/* ----------------------------------------
 * p_read_curves_from_file_gimp2_6_format
 * ----------------------------------------
 * Parse curve points from file
 * in the new # GIMP curves tool settings
 * fileformat.
 * Example content of such a file:
 * ===============================
 *     # GIMP curves tool settings
 *     (time 0)
 *     (channel value)
 *     (curve
 *         (curve-type smooth)
 *         (n-points 17)
 *         (points 34 0.000000 0.000000 -1.000000 -1.000000 ....
 *         .... -1.000000 -1.000000 1.000000 1.000000)
 *         (n-samples 256)
 *         (samples 256 0.000000 0.003922 0.007843 0.011765 0.015686 .....
 *          ...... 0.988235 0.992157 0.996078 1.000000))
 *     (time 0)
 *     (channel red)    (curve  ............)
 *     (channel green)  (curve  ............)
 *     (channel blue)   (curve  ............)
 *     (channel alpha)  (curve  ............)
 *
 *
 * opposite to the 2.4 format, there is no need to calculate the 256 points in the 2.6 format.
 * that typically are already provides all 256 points for all 5 channels.
 *
 * RESTRICTIONS:
 * ------------
 * the parser is limited to read only the 256 sample points for the 5 channels
 *   value, red, green, blue, alpha
 * that are required for the non-interactive calls (see also: p_gimp_curves_explicit)
 * all other tokens in the file are ignored. (even the n-samples token is ignored)
 * load will fail in case the file does not contain the expected 256 sample points per channel.
 */
static gboolean
p_read_curves_from_file_gimp2_6_format (const char *filename, FILE *fp, wr_curves_val_t *cuvals, gchar *buf)
{
  int ii;
  gboolean success;
  gint32     l_fileSize;
  CurveParserContext *cpc;

  if(gap_debug)
  {
    printf("START p_read_curves_from_file_gimp2_6_format\n");
  }
  

  l_fileSize = gap_file_get_filesize(filename);
  if (l_fileSize > MAX_FILESIZE)
  {
    printf("ERROR: file %s length %d is larger than plausible size %d\n"
      , filename
      , l_fileSize
      , MAX_FILESIZE
      );
    return FALSE;
  }
  
  if (strcmp (buf, "# GIMP curves tool settings\n") != 0)
  {
    printf("ERROR: file %s does not start with '%s'\n", filename, "# GIMP curves tool settings");
    return FALSE;
  }
  
  success = TRUE;
  cpc = p_new_CurveParserContext(l_fileSize, fp, cuvals);

  while(*cpc->ptr != '\0')
  {
    p_skip_whitespace(cpc);
    p_skip_until_opening_bracket(cpc);
    if(gap_debug)
    {
      printf("\n\nCurves file cpc->ptr:%80.80s\n", cpc->ptr);
    }

    if(strncmp(cpc->ptr, TOKEN_CHANNEL, strlen(TOKEN_CHANNEL)) == 0)
    {
       p_read_channel(cpc);
    } 
    else if(strncmp(cpc->ptr, TOKEN_CURVE, strlen(TOKEN_CURVE)) == 0)
    {
       p_read_curve(cpc);
    }
    else
    {
       p_skip_until_closing_bracket(cpc);
    }
  }

  /* check if samples for all channels have been loaded successfully */
  for(ii=0; ii < MAX_CHANNELS; ii++)
  {
    if ((cpc->channel_found[ii] != TRUE) || (cpc->samples_found[ii] != TRUE))
    {
      success = FALSE;
      break;
    }
  }
  
  g_free(cpc->buffer);
  g_free(cpc);
  
  
  return success;
  
}  /* end p_read_curves_from_file_gimp2_6_format */


/* ----------------------------------------
 * p_load_curve
 * ----------------------------------------
 * load curve from file, 
 *  supports curve file formats of the gimp-2.4.x and gimp-2.6.x releases
 */
static int
p_load_curve(gchar *filename, wr_curves_val_t *cuvals)
{
  FILE  *fp;

  fp = g_fopen (filename, "rt");

  if (!fp)
  {
          g_message (_("Unable to open file %s"), filename);
          return -1;
  }

  if (!read_curves_from_file (filename, fp, cuvals))
  {
          g_message (("Error in reading file %s"), filename);
          fclose (fp);
          return -1;
  }

  fclose (fp);
  return 0;
}


/* ----------------------------------------
 * p_delta_pointval_t
 * ----------------------------------------
 * Delta Calculations for the iterator
 */
static void
p_delta_pointval_t (pointval_t  *val,
                pointval_t   val_from,
                pointval_t   val_to,
                 gint32   total_steps,
                 gdouble  current_step)
{
    gdouble     delta;

    if(total_steps < 1) return;

    delta = ((gdouble)(val_to - val_from) / (gdouble)total_steps) * ((gdouble)total_steps - current_step);
    *val  = val_from + delta;
}


/* ----------------------------------------
 * p_gimp_curves_explicit
 * ----------------------------------------
 * PDB call of the curves feature of the gimp color curves tool
 * for one channel.
 */
static gint32
p_gimp_curves_explicit (gint32 drawable_id,
                   gint32 channel,
                   gint32 num_bytes,
                   gint8 *curve_points)
{
   static char     *l_procname = "gimp_curves_explicit";
   GimpParam          *return_vals;
   int              nreturn_vals;

   return_vals = gimp_run_procedure (l_procname,
                                 &nreturn_vals,
                                 GIMP_PDB_DRAWABLE,  drawable_id,
                                 GIMP_PDB_INT32,     channel,
                                 GIMP_PDB_INT32,     num_bytes,
                                 GIMP_PDB_INT8ARRAY, curve_points,
                                 GIMP_PDB_END);

   if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
   {
      gimp_destroy_params(return_vals, nreturn_vals);
      return (0);
   }
   gimp_destroy_params(return_vals, nreturn_vals);
   printf("Error: PDB call of %s failed status:%d\n", l_procname, (int)return_vals[0].data.d_status);
   return(-1);
}       /* end p_gimp_curves_explicit */


/* ----------------------------------------
 * p_run_curves_tool
 * ----------------------------------------
 * run the curves feature of the gimp color curves tool
 * for all relevant histogram channels
 */
static void
p_run_curves_tool(gint32 drawable_id, wr_curves_val_t *cuvals)
{
  gint32  l_idx;
  gint32  l_channel;
  gint32  l_channel_histogram;
  gint8 curve_points[256];

  for(l_channel=4; l_channel >= 0; l_channel--)
  {
     gboolean isChannelRelevant;
     
     for(l_idx=0; l_idx < 256; l_idx++)
     {
       /* curve_points[l_idx] = (gint8)ROUND(cuvals->val_curve[l_channel][l_idx]); */
       curve_points[l_idx] = (gint8)cuvals->val_curve[l_channel][l_idx];
     }
     
     l_channel_histogram = l_channel;
     isChannelRelevant = FALSE;
     
     switch (l_channel)
     {
       case 0:   /* HISTOGRAM-VALUE */
         if (gimp_drawable_is_gray(drawable_id))
         {
           isChannelRelevant = TRUE;
         }
         else if (gimp_drawable_is_rgb(drawable_id))
         {
           isChannelRelevant = TRUE;
         }
         break;
       case 1:   /* HISTOGRAM-RED   */
       case 2:   /* HISTOGRAM-GREEN */
       case 3:   /* HISTOGRAM-BLUE  */
         if (gimp_drawable_is_rgb(drawable_id))
         {
           isChannelRelevant = TRUE;
         }
         break;
       case 4:   /* HISTOGRAM-ALPHA */
         if (gimp_drawable_has_alpha(drawable_id))
         {
           isChannelRelevant = TRUE;
         }
         break;
       default:
         isChannelRelevant = FALSE;
         break;
     
     }
     
     if (isChannelRelevant)
     {
       p_gimp_curves_explicit(drawable_id, l_channel_histogram, 256, curve_points);
     }
  }
}



/*
 * ===== START of DIALOG and callback stuff ===========
 */

/* ---------------------------------
 * wr_curve_load_callback
 * ---------------------------------
 */
static void
p_filesel_close_cb (GtkWidget *widget,
                    gpointer   data)
{
  WrCurveDialog *wcd;

  wcd = (WrCurveDialog *) data;

  if(wcd->filesel == NULL) return;

  gtk_widget_destroy(GTK_WIDGET(wcd->filesel));
  wcd->filesel = NULL;   /* now filesel is closed */
}


/* ---------------------------------
 * wr_curve_load_callback
 * ---------------------------------
 */
static void
p_filesel_ok_callback (GtkWidget *widget,
                         gpointer   data)
{
  WrCurveDialog *wcd;
  wr_curves_val_t  cuvals;

  wcd = (WrCurveDialog *) data;

  if(wcd->filesel == NULL) return;

  if(wcd->filename) g_free(wcd->filename);

  wcd->filename = g_strdup(gtk_file_selection_get_filename (GTK_FILE_SELECTION (wcd->filesel)));
  gtk_entry_set_text(GTK_ENTRY(wcd->entry), wcd->filename);

  gtk_widget_destroy(GTK_WIDGET(wcd->filesel));
  wcd->filesel = NULL;

  /* try to read the file,
   * (and make a g_message on errors)
   */
  p_load_curve(wcd->filename, &cuvals);
}

/* ---------------------------------
 * wr_curve_load_callback
 * ---------------------------------
 */
static void
wr_curve_load_callback (GtkWidget *w,
                      WrCurveDialog *wcd)
{
  GtkWidget *filesel;

  if(wcd == NULL)
  {
    return;
  }
  if(wcd->filesel != NULL)
  {
     gtk_window_present(GTK_WINDOW(wcd->filesel));
     return;   /* filesel is already open */
  }
  
  filesel = gtk_file_selection_new ( _("Load color curve from file"));
  wcd->filesel = filesel;

  gtk_window_set_position (GTK_WINDOW (filesel), GTK_WIN_POS_MOUSE);
  g_signal_connect (GTK_FILE_SELECTION (filesel)->ok_button,
                    "clicked", (GtkSignalFunc) p_filesel_ok_callback,
                    wcd);
  g_signal_connect (GTK_FILE_SELECTION (filesel)->cancel_button,
                      "clicked", (GtkSignalFunc) p_filesel_close_cb,
                      wcd);
  g_signal_connect (filesel, "destroy",
                    (GtkSignalFunc) p_filesel_close_cb,
                    wcd);
  if(wcd->filename)
  {
    gtk_file_selection_set_filename (GTK_FILE_SELECTION (filesel),
                                     wcd->filename);
  }
  gtk_widget_show (filesel);

}

/* ---------------------------------
 * text_entry_callback
 * ---------------------------------
 */
static void
text_entry_callback(GtkWidget *widget, WrCurveDialog *wcd)
{
  if(wcd)
  {
    if(wcd->filename) g_free(wcd->filename);
    wcd->filename = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));
  }
}

/* ---------------------------------
 * wr_curve_response
 * ---------------------------------
 */
static void
wr_curve_response (GtkWidget *widget,
                 gint       response_id,
                 WrCurveDialog *wcd)
{
  GtkWidget *dialog;

  switch (response_id)
  {
    case GTK_RESPONSE_OK:
      if(wcd)
      {
        if (GTK_WIDGET_VISIBLE (wcd->shell))
          gtk_widget_hide (wcd->shell);

        wcd->run = TRUE;
      }

    default:
      dialog = NULL;
      if(wcd)
      {
        dialog = wcd->shell;
        if(dialog)
        {
          wcd->shell = NULL;
          gtk_widget_destroy (dialog);
        }
      }
      gtk_main_quit ();
      break;
  }
}  /* end wr_curve_response */



WrCurveDialog *
do_dialog (wr_curves_val_t *cuvals)
{
  WrCurveDialog *wcd;
  GtkWidget *dialog;
  GtkWidget  *vbox;
  GtkWidget  *hbox;
  GtkWidget  *button;
  GtkWidget  *entry;


  /* Init UI  */
  gimp_ui_init ("wr_curves", FALSE);
  gap_stock_init();


  /*  The curve_bend dialog  */
  wcd = g_malloc (sizeof (WrCurveDialog));
  wcd->run = FALSE;
  wcd->filesel = NULL;

  /*  The dialog and main vbox  */
  dialog = gimp_dialog_new (_("CurvesFile"), "curves_wrapper",
                               NULL, 0,
                               gimp_standard_help_func, PLUG_IN_HELP_ID,

                               GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                               GTK_STOCK_OK,     GTK_RESPONSE_OK,
                               NULL);

  wcd->shell = dialog;
  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (wr_curve_response),
                    wcd);

  /* the vbox */
  vbox = gtk_vbox_new (FALSE, 2);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox,
                      TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  /* the hbox */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);

  /*  The Load button  */
  button = gtk_button_new_with_label (_("Load Curve"));
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (wr_curve_load_callback),
                    wcd);
  gtk_widget_show (button);
  gimp_help_set_help_data (button,
                           _("Load curve from a GIMP curve file "
                             "(that was saved with the GIMP's color curve tool)"),
                           NULL);

  /*  The filename entry */
  entry = gtk_entry_new();
  gtk_widget_set_size_request(entry, 350, -1);
  gtk_entry_set_text(GTK_ENTRY(entry), "");
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
  g_signal_connect(G_OBJECT(entry), "changed",
                   G_CALLBACK (text_entry_callback),
                   wcd);
  gtk_widget_show (entry);
  wcd->entry = entry;

  gtk_widget_show (dialog);

  gtk_main ();
  gdk_flush ();


  return wcd;
}




MAIN ()


/* ----------------------------------------
 * query
 * ----------------------------------------
 * register as PDB procedure (both the wrapper and iterator)
 */
static void
query (void)
{
  static GimpParamDef args[] = {
                  { GIMP_PDB_INT32,      "run_mode", "Interactive, non-interactive"},
                  { GIMP_PDB_IMAGE,      "image", "Input image" },
                  { GIMP_PDB_DRAWABLE,   "drawable", "Input drawable (must be a layer)"},
                  { GIMP_PDB_STRING,     "filename", "Name of a #GIMP curves file (saved by the original Curve Tool)"},
  };
  static int nargs = sizeof(args) / sizeof(args[0]);

  static GimpParamDef return_vals[] =
  {
    { GIMP_PDB_DRAWABLE, "the_drawable", "the handled drawable" }
  };
  static int nreturn_vals = sizeof(return_vals) / sizeof(return_vals[0]);


  static GimpParamDef args_iter[] =
  {
    {GIMP_PDB_INT32, "run_mode", "non-interactive"},
    {GIMP_PDB_INT32, "total_steps", "total number of steps (# of layers-1 to apply the related plug-in)"},
    {GIMP_PDB_FLOAT, "current_step", "current (for linear iterations this is the layerstack position, otherwise some value inbetween)"},
    {GIMP_PDB_INT32, "len_struct", "length of stored data structure with id is equal to the plug_in  proc_name"},
  };
  static int nargs_iter = sizeof(args_iter) / sizeof(args_iter[0]);

  static GimpParamDef *return_iter = NULL;
  static int nreturn_iter = 0;

  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);

  /* the actual installation of the bend plugin */
  gimp_install_procedure (PLUG_IN_NAME,
                          PLUG_IN_DESCRIPTION,
                         "This Plugin loads a # GIMP Curves File,"
                         " that was saved by the GIMP 2.0pre1 Curves Tool"
                         " then calculates the curves (256 points foreach channel val,r,g,b,a)"
                         " and calls the Curve Tool via PDB interface with the calculated curve points"
                         " It also stores the points, and offers a GIMP_RUN_WITH_LAST_VALUES"
                         " Interface and an Iterator Procedure for animated calls"
                         " of the Curves Tool with varying values"
                          ,
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_("CurvesFile..."),
                          PLUG_IN_IMAGE_TYPES,
                          GIMP_PLUGIN,
                          nargs,
                          nreturn_vals,
                          args,
                          return_vals);


  /* the installation of the Iterator extension for the bend plugin */
  gimp_install_procedure (PLUG_IN_ITER_NAME,
                          "This extension calculates the modified values for one iterationstep for the call of plug_in_curve_bend",
                          "",
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          NULL,    /* do not appear in menus */
                          NULL,
                          GIMP_PLUGIN,
                          nargs_iter, nreturn_iter,
                          args_iter, return_iter);
  {
    /* Menu names */
    const char *menupath_image_video_layer_colors = N_("<Image>/Video/Layer/Colors/");

    //gimp_plugin_menu_branch_register("<Image>", "Video");
    //gimp_plugin_menu_branch_register("<Image>/Video", "Layer");
    //gimp_plugin_menu_branch_register("<Image>/Video/Layer", "Colors");

    gimp_plugin_menu_register (PLUG_IN_NAME, menupath_image_video_layer_colors);
  }
}


/* ----------------------------------------
 * run
 * ----------------------------------------
 * the main entry point when the filter is started via the GIMP menu.
 */

static void  
run(const gchar *name
   , gint nparams
   , const GimpParam *param
   , gint *nreturn_vals
   , GimpParam **return_vals)
{
  wr_curves_val_t l_cuvals;
  WrCurveDialog   *wcd = NULL;

  gint32    l_image_id = -1;
  gint32    l_drawable_id = -1;
  gint32    l_handled_drawable_id = -1;

  /* Get the runmode from the in-parameters */
  GimpRunMode run_mode = param[0].data.d_int32;

  /* status variable, use it to check for errors in invocation usualy only
     during non-interactive calling */
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;

  /*always return at least the status to the caller. */
  static GimpParam values[2];


  INIT_I18N();

  /* initialize the return of the status */
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
  values[1].type = GIMP_PDB_DRAWABLE;
  values[1].data.d_int32 = -1;
  *nreturn_vals = 2;
  *return_vals = values;

  /* the Iterator Stuff */
  if (strcmp (name, PLUG_IN_ITER_NAME) == 0)
  {
     gint32  len_struct;
     gint32  total_steps;
     gdouble current_step;
     wr_curves_val_t   cval;                  /* current values while iterating */
     wr_curves_val_t   cval_from, cval_to;    /* start and end values */
     gint  l_idi, l_idx;

     /* Iterator procedure for animated calls is usually called from
      * "plug_in_gap_layers_run_animfilter"
      * (always run noninteractive)
      */
     if ((run_mode == GIMP_RUN_NONINTERACTIVE) && (nparams == 4))
     {
       total_steps  =  param[1].data.d_int32;
       current_step =  param[2].data.d_float;
       len_struct   =  param[3].data.d_int32;

       if(len_struct == sizeof(cval))
       {
         /* get _FROM and _TO data,
          * This data was stored by plug_in_gap_layers_run_animfilter
          */
          gimp_get_data(PLUG_IN_DATA_ITER_FROM, &cval_from);
          gimp_get_data(PLUG_IN_DATA_ITER_TO,   &cval_to);
          memcpy(&cval, &cval_from, sizeof(cval));
          for(l_idi = 0; l_idi < 5; l_idi++)
          {
             for(l_idx = 0; l_idx < 256; l_idx++)
             {
               p_delta_pointval_t(&cval.val_curve[l_idi][l_idx],
                    cval_from.val_curve[l_idi][l_idx],
                    cval_to.val_curve[l_idi][l_idx],
                    total_steps, current_step);
             }
          }
          gimp_set_data(PLUG_IN_NAME, &cval, sizeof(cval));
       }
       else status = GIMP_PDB_CALLING_ERROR;
     }
     else status = GIMP_PDB_CALLING_ERROR;

     values[0].data.d_status = status;
     return;
  }




  /* get image and drawable */
  l_image_id = param[1].data.d_int32;
  l_drawable_id = param[2].data.d_drawable;

  if(status == GIMP_PDB_SUCCESS)
  {
    /* how are we running today? */
    switch (run_mode)
     {
      case GIMP_RUN_INTERACTIVE:
        /* Get information from the dialog */
        wcd = do_dialog(&l_cuvals);
        wcd->show_progress = TRUE;
        if(wcd->filename == NULL)
        {
           status = GIMP_PDB_EXECUTION_ERROR;
        }
        else
        {
          if(p_load_curve(wcd->filename, &l_cuvals) < 0)
          {
            status = GIMP_PDB_EXECUTION_ERROR;
          }
        }
        break;

      case GIMP_RUN_NONINTERACTIVE:
        /* check to see if invoked with the correct number of parameters */
        if (nparams >= 4)
        {
           wcd = g_malloc (sizeof (WrCurveDialog));
           wcd->run = TRUE;
           wcd->show_progress = FALSE;

           wcd->filename = g_strdup(param[3].data.d_string);
        }
        else
        {
          status = GIMP_PDB_CALLING_ERROR;
        }

        break;

      case GIMP_RUN_WITH_LAST_VALS:
        wcd = g_malloc (sizeof (WrCurveDialog));
        wcd->run = TRUE;
        wcd->show_progress = TRUE;
        /* Possibly retrieve data from a previous run */
        gimp_get_data (PLUG_IN_NAME, &l_cuvals);
        break;

      default:
        break;
    }
  }

  if (wcd == NULL)
  {
    status = GIMP_PDB_EXECUTION_ERROR;
  }

  if (status == GIMP_PDB_SUCCESS)
  {
     /* Run the main function */
     if(wcd->run)
     {
        gimp_image_undo_group_start(l_image_id);
        p_run_curves_tool(l_drawable_id, &l_cuvals);
        l_handled_drawable_id = l_drawable_id;
        gimp_image_undo_group_end(l_image_id);

        /* Store variable states for next run */
        if (run_mode == GIMP_RUN_INTERACTIVE)
        {
          gimp_set_data(PLUG_IN_NAME, &l_cuvals, sizeof(l_cuvals));
        }
     }
     else
     {
       status = GIMP_PDB_EXECUTION_ERROR;       /* dialog ended with cancel button */
     }

     /* If run mode is interactive, flush displays, else (script) don't
        do it, as the screen updates would make the scripts slow */
     if (run_mode != GIMP_RUN_NONINTERACTIVE)
     {
       gimp_displays_flush ();
     }
  }
  values[0].data.d_status = status;
  values[1].data.d_int32 = l_handled_drawable_id;   /* return the id of handled layer */

}       /* end run */
