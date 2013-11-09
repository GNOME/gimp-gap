/*  gap_story_syntax.c
 *
 *  This module handles GAP storyboard file syntax list for
 *  named parameters parsing support of storyboard level1 files
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
 * version 2.1.0a;     2004/04/24  hof: created
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "gap_story_syntax.h"

typedef struct GapStbSyntaxParElem {
  gint      par_idx;
  gchar    *parname;
  void     *next;
} GapStbSyntaxParElem;


typedef struct GapStbSyntaxElem {
  gchar                 *record_key;  /* keyword record identifier string */
  GapStbSyntaxParElem   *par_list;
  void                  *next;
} GapStbSyntaxElem;



static GapStbSyntaxElem  *global_syntax_par_list = NULL;

static void  p_add_keyword(const char *record_key
              , ...
              );
static void  p_create_syntax_list(void);


/* -------------------------------------
 * p_add_keyword
 * -------------------------------------
 */
static void
p_add_keyword(const char *record_key
             , ...
             )
{
  va_list         args;
  const char *parname;
  GapStbSyntaxElem    *sxpar_elem;
  GapStbSyntaxParElem *par_elem;
  gint par_idx;

  sxpar_elem = g_new(GapStbSyntaxElem, 1);
  sxpar_elem->record_key = g_strdup(record_key);
  sxpar_elem->par_list = NULL;
  sxpar_elem->next = global_syntax_par_list;
  global_syntax_par_list = sxpar_elem;

  par_idx = 1;

  va_start (args, record_key);
  parname = va_arg (args,  const char *);
  while (parname)
  {
    par_elem = g_new(GapStbSyntaxParElem, 1);
    par_elem->par_idx = par_idx;
    par_idx++;
    par_elem->parname = g_strdup(parname);
    par_elem->next = sxpar_elem->par_list;
    sxpar_elem->par_list = par_elem;

    parname = va_arg (args,  const char *);
  }
  va_end (args);

}  /* end p_add_keyword */



/* -------------------------------------
 * gap_stb_syntax_get_parname_idx
 * -------------------------------------
 * return the expected postion of the parameter
 * with name == keyword for the record type specified by record_key
 * return -1 if the specified record type has no paramter name == keyword
 */
gint
gap_stb_syntax_get_parname_idx(const char *record_key
                 ,const char *parname
                 )
{
  GapStbSyntaxElem  *sp_elem;

  if(global_syntax_par_list == NULL)
  {
    p_create_syntax_list();
  }

  if((record_key == NULL)
  || (parname == NULL))
  {
    return(-1);
  }


  for(sp_elem = global_syntax_par_list; sp_elem != NULL; sp_elem = (GapStbSyntaxElem  *)sp_elem->next)
  {
    if(strcmp(record_key, sp_elem->record_key) == 0)
    {
      GapStbSyntaxParElem *par;

      for(par=sp_elem->par_list; par != NULL; par = (GapStbSyntaxParElem *)par->next)
      {
        if(strcmp(par->parname, parname) == 0)
        {
          return(par->par_idx);
        }
      }
      return(-1);
    }
  }
  return(-1);
}  /* end gap_stb_syntax_get_parname_idx */



/* -------------------------------------
 * gap_stb_syntax_get_parname
 * -------------------------------------
 * return the parameter name for the given recodrd_key and index
 * return NULL if no param found.
 * the caller must g_free the returned string
 */
char *
gap_stb_syntax_get_parname(const char *record_key
                 ,gint par_idx
                 )
{
  GapStbSyntaxElem  *sp_elem;

  if(global_syntax_par_list == NULL)
  {
    p_create_syntax_list();
  }

  if(record_key == NULL)
  {
    return(NULL);
  }

  for(sp_elem = global_syntax_par_list; sp_elem != NULL; sp_elem = (GapStbSyntaxElem  *)sp_elem->next)
  {
    if(strcmp(record_key, sp_elem->record_key) == 0)
    {
      GapStbSyntaxParElem *par;

      for(par=sp_elem->par_list; par != NULL; par = (GapStbSyntaxParElem *)par->next)
      {
        if(par->par_idx == par_idx)
        {
          if(par->parname)
          {
            return(g_strdup(par->parname));
          }
          return(NULL);
        }
      }
      return(NULL);
    }
  }
  return(NULL);
}  /* end gap_stb_syntax_get_parname */


/* -------------------------------------
 * gap_stb_syntax_get_parname_tab
 * -------------------------------------
 */
void
gap_stb_syntax_get_parname_tab(const char *record_key
                 ,GapStbSyntaxParnames *par_tab
                 )
{
  GapStbSyntaxElem  *sp_elem;

  par_tab->tabsize = 0;
  if(global_syntax_par_list == NULL)
  {
    p_create_syntax_list();
  }

  if(record_key == NULL)
  {
    return;
  }

  for(sp_elem = global_syntax_par_list; sp_elem != NULL; sp_elem = (GapStbSyntaxElem  *)sp_elem->next)
  {
    if(strcmp(record_key, sp_elem->record_key) == 0)
    {
      GapStbSyntaxParElem *par;

      for(par=sp_elem->par_list; par != NULL; par = (GapStbSyntaxParElem *)par->next)
      {
        par_tab->parname[par->par_idx] = par->parname;
        par_tab->tabsize++;
      }
      return;
    }
  }
  return;
}  /* end gap_stb_syntax_get_parname_tab */


/* -------------------------------------
 * p_create_syntax_list
 * -------------------------------------
 */
static void
p_create_syntax_list(void)
{
  p_add_keyword(GAP_STBKEY_STB_HEADER
               ,"version"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_CLIP_HEADER
               ,"version"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_LAYOUT_SIZE
               ,"cols"
               ,"rows"
               ,"thumbsize"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_EDIT_SETTINGS
               ,"secition_name"
               ,"track"
               ,"page"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_MASTER_SIZE
               ,"width"
               ,"height"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_MASTER_FRAMERATE
               ,"frames_per_sec"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_MASTER_LAYERSTACK
               ,"track1"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_MASTER_FRAME_ASPECT
               ,"width"
               ,"height"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_PREFERRED_DECODER
               ,"decoder"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_MASTER_INSERT_ALPHA
               ,"format"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_MASTER_INSERT_AREA
               ,"format"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_MAIN_SECTION
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_SUB_SECTION
               ,"section_name"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_PLAY_SECTION
               ,"track"
               ,"section_name"
               ,"from"
               ,"to"
               ,"mode"
               ,"nloops"
               ,"stepsize"
               ,"macro"
               ,"flip"
               ,"mask_name"
               ,"mask_anchor"
               ,"mask_stepsize"
               ,"mask_disable"
               ,"macsteps"
               ,"macaccel"
               ,"colormask_file"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_PLAY_BLACKSECTION
               ,"track"
               ,"section_name"
               ,"from"
               ,"to"
               ,"mode"
               ,"nloops"
               ,"stepsize"
               ,"macro"
               ,"flip"
               ,"mask_name"
               ,"mask_anchor"
               ,"mask_stepsize"
               ,"mask_disable"
               ,"macsteps"
               ,"macaccel"
               ,"colormask_file"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_PLAY_MOVIE
               ,"track"
               ,"file"
               ,"from"
               ,"to"
               ,"mode"
               ,"nloops"
               ,"seltrack"
               ,"exactseek"
               ,"deinterlace"
               ,"stepsize"
               ,"macro"
               ,"decoder"
               ,"flip"
               ,"mask_name"
               ,"mask_anchor"
               ,"mask_stepsize"
               ,"mask_disable"
               ,"macsteps"
               ,"macaccel"
               ,"colormask_file"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_PLAY_FRAMES
               ,"track"
               ,"base"
               ,"ext"
               ,"from"
               ,"to"
               ,"mode"
               ,"nloops"
               ,"stepsize"
               ,"macro"
               ,"flip"
               ,"mask_name"
               ,"mask_anchor"
               ,"mask_stepsize"
               ,"mask_disable"
               ,"macsteps"
               ,"macaccel"
               ,"colormask_file"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_PLAY_ANIMIMAGE
               ,"track"
               ,"file"
               ,"from"
               ,"to"
               ,"mode"
               ,"nloops"
               ,"stepsize"
               ,"macro"
               ,"flip"
               ,"mask_name"
               ,"mask_anchor"
               ,"mask_stepsize"
               ,"mask_disable"
               ,"macsteps"
               ,"macaccel"
               ,"colormask_file"
               ,NULL
               );

  p_add_keyword(GAP_STBKEY_VID_PLAY_IMAGE
               ,"track"
               ,"file"
               ,"nloops"
               ,"macro"
               ,"flip"
               ,"mask_name"
               ,"mask_anchor"
               ,"mask_stepsize"
               ,"mask_disable"
               ,"macsteps"
               ,"macaccel"
               ,"colormask_file"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_PLAY_COLOR
               ,"track"
               ,"red"
               ,"green"
               ,"blue"
               ,"alpha"
               ,"nloops"
               ,"macro"
               ,"flip"
               ,"mask_name"
               ,"mask_anchor"
               ,"mask_stepsize"
               ,"mask_disable"
               ,"macsteps"
               ,"macaccel"
               ,"colormask_file"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_SILENCE
               ,"track"
               ,"nloops"
               ,"wait_until_sec"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_ROTATE
               ,"track"
               ,"rotate_from"
               ,"rotate_to"
               ,"nframes"
               ,"accel"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_OPACITY
               ,"track"
               ,"opacity_from"
               ,"opacity_to"
               ,"nframes"
               ,"accel"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_ZOOM_X
               ,"track"
               ,"zoom_x_from"
               ,"zoom_x_to"
               ,"nframes"
               ,"accel"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_ZOOM_Y
               ,"track"
               ,"zoom_y_from"
               ,"zoom_y_to"
               ,"nframes"
               ,"accel"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_MOVE_X
               ,"track"
               ,"move_x_from"
               ,"move_x_to"
               ,"nframes"
               ,"accel"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_MOVE_Y
               ,"track"
               ,"move_y_from"
               ,"move_y_to"
               ,"nframes"
               ,"accel"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_FIT_SIZE
               ,"track"
               ,"mode"
               ,"proportions"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_OVERLAP
               ,"track"
               ,"nframes"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_VID_MOVEPATH
               ,"track"
               ,"frame_from"
               ,"frame_to"
               ,"nframes"
               ,"accel"
               ,"xml_paramfile"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_MASK_MOVIE
               ,"mask_name"
               ,"file"
               ,"from"
               ,"to"
               ,"seltrack"
               ,"exactseek"
               ,"deinterlace"
               ,"decoder"
               ,"flip"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_MASK_BLACKSECTION
               ,"mask_name"
               ,"from"
               ,"to"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_MASK_FRAMES
               ,"mask_name"
               ,"base"
               ,"ext"
               ,"from"
               ,"to"
               ,"flip"
              ,NULL
               );
  p_add_keyword(GAP_STBKEY_MASK_ANIMIMAGE
               ,"mask_name"
               ,"file"
               ,"from"
               ,"to"
               ,"flip"
               ,NULL
               );

  p_add_keyword(GAP_STBKEY_MASK_IMAGE
               ,"mask_name"
               ,"file"
               ,"flip"
               ,NULL
               );



  p_add_keyword(GAP_STBKEY_AUD_MASTER_VOLUME
               ,"volume"
                ,NULL
               );
  p_add_keyword(GAP_STBKEY_AUD_MASTER_SAMPLERATE
               ,"samples_per_sec"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_AUD_PLAY_SOUND
               ,"track"
               ,"file"
               ,"start_sec"
               ,"end_sec"
               ,"volume"
               ,"start_volume"
               ,"fade_in_time"
               ,"end_volume"
               ,"fade_out_time"
               ,"nloops"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_AUD_PLAY_MOVIE
               ,"track"
               ,"file"
               ,"start_sec"
               ,"end_sec"
               ,"volume"
               ,"start_volume"
               ,"fade_in_time"
               ,"end_volume"
               ,"fade_out_time"
               ,"nloops"
               ,"seltrack"
               ,"decoder"
               ,"from"
               ,"to"
               ,"framerate"
               ,NULL
               );
  p_add_keyword(GAP_STBKEY_AUD_SILENCE
               ,"track"
               ,"duration_sec"
               ,"wait_until_sec"
           ,NULL
               );
}  /* end p_create_syntax_list */
