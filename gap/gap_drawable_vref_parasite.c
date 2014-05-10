/* gap_drawable_vref_parasite.c
 *
 *  This module handles gap specific video reference drawable parasites.
 *  Such parasites are typically used to identify extracted video file frames
 *  for usage in filtermacro as persistent drawable_id references at recording time of filtermacros.
 *
 *  The gap player does attach such vref drawable parasites (as temporary parasites)
 *  when extracting a videoframe at original size (by click on the preview)
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>

#include "gap_drawable_vref_parasite.h"


extern      int gap_debug; /* ==0  ... dont print debug infos */

/* ------------------------------------------
 * gap_dvref_debug_print_GapDrawableVideoRef
 * ------------------------------------------
 */
void
gap_dvref_debug_print_GapDrawableVideoRef(GapDrawableVideoRef *dvref)
{
  if(gap_debug)
  {
    if(dvref == NULL)
    {
      printf("GapDrawableVideoRef: dvref:(null)\n");
      return;
    }
    if(dvref->videofile == NULL)
    {
      printf("GapDrawableVideoRef: videofile:(null)  frame:%d seltrack:%d\n"
        ,dvref->para.framenr
        ,dvref->para.seltrack
        );
      return;
    }

    printf("GapDrawableVideoRef: videofile:%s  frame:%d seltrack:%d (%s)\n"
      ,dvref->videofile
      ,dvref->para.framenr
      ,dvref->para.seltrack
      ,&dvref->para.preferred_decoder[0]
      );
  }
}  /* end gap_dvref_debug_print_GapDrawableVideoRef */


/* -------------------
 * gap_dvref_free
 * -------------------
 */
void
gap_dvref_free(GapDrawableVideoRef **dvref_ptr)
{
  GapDrawableVideoRef *dvref;
  
  dvref = *dvref_ptr;
  if (dvref)
  {
    if(dvref->videofile)
    {
      g_free(dvref->videofile);
    }
    g_free(dvref);
  }
  
  *dvref_ptr = NULL;
  
}  /* end  gap_dvref_free */

/* ---------------------------------------------------
 * gap_dvref_get_drawable_video_reference_via_parasite
 * ---------------------------------------------------
 * return Gap drawable video reference parasite if such a parasite is atached to the specified drawable_id
 *        oterwise return NULL;
 */
GapDrawableVideoRef *
gap_dvref_get_drawable_video_reference_via_parasite(gint32 drawable_id)
{
  GapDrawableVideoRef *dvref;
  GimpParasite  *l_parasite;
  
  dvref = NULL;

  l_parasite = gimp_drawable_parasite_find(drawable_id, GAP_DRAWABLE_VIDEOFILE_PARASITE_NAME);
  if(l_parasite)
  {
    if(gap_debug)
    {
        printf("gap_dvref_get_drawable_video_reference_via_parasite: size:%d  data:%s\n"
          ,l_parasite->size
          ,(char *)l_parasite->data
          );
    }

    dvref = g_new(GapDrawableVideoRef, 1);

    dvref->videofile = g_malloc0(l_parasite->size +1);
    memcpy(dvref->videofile, l_parasite->data, l_parasite->size);


    gimp_parasite_free(l_parasite);


    l_parasite = gimp_drawable_parasite_find(drawable_id, GAP_DRAWABLE_VIDEOPARAMS_PARASITE_NAME);
    if(l_parasite)
    {
      memcpy(&dvref->para, l_parasite->data, sizeof(GapDrawableVideoParasite));
      gimp_parasite_free(l_parasite);

      if(gap_debug)
      {
        printf("gap_dvref_get_drawable_video_reference_via_parasite: dvref PARASITES OK\n");
        gap_dvref_debug_print_GapDrawableVideoRef(dvref);
      }
      return (dvref);
    }

  }

  if(gap_debug)
  {
    printf("gap_dvref_get_drawable_video_reference_via_parasite: NO dvref parasites found.\n");
  }
  
  return (NULL);
}  /* end gap_dvref_get_drawable_video_reference_via_parasite */


    
/* --------------------------------------
 * gap_dvref_assign_videoref_parasites
 * --------------------------------------
 * if gpp->drawable_vref contains vaild video reference
 * then
 * assign video reference parasites to the specified drawable_id (typically this is a layer.)
 * (one parasite contains only the videfilename and has variable length,
 * the other contains framenumber and other information that was used to fetch
 * the frame from the videofile).
 *
 * the video reference is typically set on successful fetch
 * from a video file. (and reset on all other types of frame fetches)
 *
 + TODO: query gimprc parameter that can configures using persitent drawable_videoref_parasites.
 *       (default shall be temporary parasites)
 */
void
gap_dvref_assign_videoref_parasites(GapDrawableVideoRef *dvref, gint32 drawable_id)
{
  GimpParasite    *l_parasite;

  if(gap_debug)
  {
    printf("gap_assign_drawable_videoref_parasite: START\n");
    gap_dvref_debug_print_GapDrawableVideoRef(dvref);
  }

  if(dvref->videofile == NULL)
  {
    /* no viedo reference available for the current frame */
    return;
  }


  l_parasite = gimp_parasite_new(GAP_DRAWABLE_VIDEOFILE_PARASITE_NAME
                                 ,0  /* GIMP_PARASITE_PERSISTENT  0 for non persistent */
                                 ,1 + strlen(dvref->videofile)
                                 ,dvref->videofile  /* parasite data */
                                 );
  if(l_parasite)
  {
    gimp_drawable_parasite_attach(drawable_id, l_parasite);
    gimp_parasite_free(l_parasite);
  }

  l_parasite = gimp_parasite_new(GAP_DRAWABLE_VIDEOPARAMS_PARASITE_NAME
                                 ,0 /* GIMP_PARASITE_PERSISTENT */
                                 ,sizeof(GapDrawableVideoParasite)
                                 ,&dvref->para  /* parasite data */
                                 );
  if(l_parasite)
  {
    gimp_drawable_parasite_attach(drawable_id, l_parasite);
    gimp_parasite_free(l_parasite);
  }

}       /* end gap_dvref_assign_videoref_parasites */

