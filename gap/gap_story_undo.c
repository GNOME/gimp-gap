/* gap_story_undo.c
 *  This module handles GAP storyboard undo and redo features.
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
 * version 1.3.25a; 2007/10/18  hof: created
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include <gdk/gdkkeysyms.h>

#include "gap_story_main.h"
#include "gap_story_undo_types.h"
#include "gap_story_undo.h"
#include "gap_story_dialog.h"
#include "gap_story_file.h"
#include "gap_libgapbase.h"


extern int gap_debug;  /* 1 == print debug infos , 0 dont print debug infos */

static void             p_free_undo_elem(GapStoryUndoElem    *undo_elem);
static void             p_delete_redo_stack_area(GapStbTabWidgets *tabw);

/* ----------------------------------------------------
 * gap_stb_undo_debug_fprint_stack
 * ----------------------------------------------------
 */
void
gap_stb_undo_debug_fprint_stack(FILE *fp, GapStbTabWidgets *tabw)
{
  GapStoryUndoElem    *undo_elem;

  if(tabw == NULL)
  {
    fprintf(fp, "\nStack NOT present (NULL POINTER)\n");
    fflush(fp);
    return;
  }
  fprintf(fp, "\n-------------------------------- Top of STACK ---\n\n");
  fprintf(fp, "gap_stb_undo_debug_fprint_stack: tabw:%d stack_list:%d stack_ptr:%d group_counter:%.2f\n"
    , (int)tabw
    , (int)tabw->undo_stack_list
    , (int)tabw->undo_stack_ptr
    , (float)tabw->undo_stack_group_counter
    );
  fflush(fp);

  for(undo_elem = tabw->undo_stack_list; undo_elem != NULL; undo_elem = undo_elem->next)
  {
    fprintf(fp, "  addr:%d fPtr:%d %s"
          , (int)undo_elem
          , (int)undo_elem->filenamePtr
          , gap_stb_undo_feature_to_string(undo_elem->feature_id)
          );
    if(undo_elem == tabw->undo_stack_ptr)
    {
      fprintf(fp, " <-- stack_ptr");
    }
    
    if(undo_elem->fileSnapshotBefore != NULL)
    {
      fprintf(fp, " (BEFORE: fname:%s size:%d mtime:%d)"
            ,undo_elem->fileSnapshotBefore->filename
            ,(int)undo_elem->fileSnapshotBefore->filesize
            ,(int)undo_elem->fileSnapshotBefore->mtimefile
            );
    }
    if(undo_elem->fileSnapshotAfter != NULL)
    {
      fprintf(fp, " (AFTER: fname:%s size:%d mtime:%d)"
            ,undo_elem->fileSnapshotAfter->filename
            ,(int)undo_elem->fileSnapshotAfter->filesize
            ,(int)undo_elem->fileSnapshotAfter->mtimefile
            );
    }
    
    fprintf(fp, "\n");
    fflush(fp);
  }

  fprintf(fp, "\n-------------------------------- End of STACK ---\n\n");
  fflush(fp);

}  /* end gap_stb_undo_debug_fprint_stack */


/* ----------------------------------------------------
 * gap_stb_undo_feature_to_string
 * ----------------------------------------------------
 */
const char *
gap_stb_undo_feature_to_string(GapStoryFeatureEnum feature_id)
{
  switch(feature_id)
  {
    case GAP_STB_FEATURE_LATEST:                  return("latest"); break;
    case GAP_STB_FEATURE_EDIT_CUT:                return("edit cut"); break;
    case GAP_STB_FEATURE_EDIT_PASTE:              return("edit paste"); break;
    case GAP_STB_FEATURE_DND_CUT:                 return("dnd cut"); break;
    case GAP_STB_FEATURE_DND_PASTE:               return("dnd paste"); break;
    case GAP_STB_FEATURE_CREATE_CLIP:             return("create clip"); break;
    case GAP_STB_FEATURE_CREATE_TRANSITION:       return("create transition"); break;
    case GAP_STB_FEATURE_CREATE_SECTION_CLIP:     return("create section clip"); break;
    case GAP_STB_FEATURE_CREATE_SECTION:          return("create section"); break;
    case GAP_STB_FEATURE_DELETE_SECTION:          return("delete section"); break;
    case GAP_STB_FEATURE_PROPERTIES_CLIP:         return("properties clip"); break;
    case GAP_STB_FEATURE_PROPERTIES_TRANSITION:   return("properties transition"); break;
    case GAP_STB_FEATURE_PROPERTIES_SECTION:      return("properties section"); break;
    case GAP_STB_FEATURE_PROPERTIES_MASTER:       return("properties master"); break;
    case GAP_STB_FEATURE_SCENE_SPLITTING:         return("scene split"); break;
    case GAP_STB_FEATURE_GENERATE_OTONE:          return("generate otone"); break;
  }
  return("unknown");
}  /* end gap_stb_undo_feature_to_string */


/* ---------------------------------------
 * p_free_file_snapshot
 * ---------------------------------------
 */
static void
p_free_file_snapshot(GapStoryUndoFileSnapshot *fileSnapshot)
{
  if(fileSnapshot == NULL)
  {
    return;
  }
  if(fileSnapshot->filename)
  {
    g_free(fileSnapshot->filename);
  }
  if(fileSnapshot->filecontent)
  {
    g_free(fileSnapshot->filecontent);
  }
  g_free(fileSnapshot);

}  /* end p_free_file_snapshot */


/* -----------------------------------------
 * p_create_file_snapshot
 * -----------------------------------------
 */
static GapStoryUndoFileSnapshot*
p_create_file_snapshot(char *filename)
{
  GapStoryUndoFileSnapshot *fileSnapshot;
  const char               *filecontent;

  fileSnapshot = NULL;
  
  if(g_file_test(filename, G_FILE_TEST_IS_REGULAR))
  {
    gint32 filesize;
    
    filecontent = gap_file_load_file_len(filename, &filesize);
  
    if(filecontent != NULL)
    {
      fileSnapshot = g_new(GapStoryUndoFileSnapshot, 1);
      fileSnapshot->mtimefile   = gap_file_get_mtime(filename);
      fileSnapshot->filename    = g_strdup(filename);
      fileSnapshot->filecontent = filecontent;
      fileSnapshot->filesize    = filesize;
    }
  }
  
  return (fileSnapshot);
}  /* end p_create_file_snapshot */



/* -----------------------------------------
 * p_replace_file_from_snapshot
 * -----------------------------------------
 */
static void
p_replace_file_from_snapshot(GapStoryUndoFileSnapshot* fileSnapshot)
{
  FILE *fp;
  
  if(fileSnapshot == NULL)
  {
    return;
  }
  if(fileSnapshot->filename == NULL)
  {
    return;
  }
  
  if(g_file_test(fileSnapshot->filename, G_FILE_TEST_IS_REGULAR))
  {
    if(fileSnapshot->mtimefile == gap_file_get_mtime(fileSnapshot->filename))
    {
      /* the file still exists with same mtime stamp
       * no further action required in this case..
       */
      if(gap_debug)
      {
        printf("p_replace_file_from_snapshot:%s skipped due to EQUAL mtime:%d\n"
            , fileSnapshot->filename
            ,(int)fileSnapshot->mtimefile
            );
      }
      return;
    }
  }
  
  if(gap_debug)
  {
    printf("p_replace_file_from_snapshot:%s\n", fileSnapshot->filename);
  }

  /* open write binary (create or replace) */
  fp = g_fopen(fileSnapshot->filename, "wb");
  if(fp != NULL)
  {
    fwrite(fileSnapshot->filecontent,  fileSnapshot->filesize, 1, fp);
    fclose(fp);
    /* refresh the modification timestamp after rewrite */
    fileSnapshot->mtimefile = gap_file_get_mtime(fileSnapshot->filename);
  }
  

}  /* end p_replace_file_from_snapshot */







/* ---------------------------------------
 * gap_stb_undo_pop
 * ---------------------------------------
 * this procedure is typically called for UNDO storyboard changes
 * made by the storyboard features.
 *
 * undo the feature that is refered by the undo stackpointer
 * move the stackpointer to next element. (keep the element
 * on the stack for redo purpose)
 * returns a duplicate of the storyboard backup that is attached to the
 * undo stackpointer (position as it was before this call)
 * this refers to the backup version at the time BEFORE the
 * feature (that now shall be undone) was applied.
 *
 * example1:
 *                                                   latest
 * stack_list --> BBBB <-- stack_ptr before pop      BBBB 
 *                AAAA                               AAAA <--- stack_ptr after pop returns (BBBB)
 * --------------------------------------------------------------------------------
 *
 * if the stack_ptr is on top (e.g. == stack_list root)
 * a duplicate of the current storyboard is pushed as feature_id "latest" onto the stack.
 * this backup reprents the version AFTER feature BBBB of example1 to be re-done
 *
 *           
 * example2:
 *
 * stack_list -->latest                             latest 
 *               DDDD                               DDDD 
 *               CCCC  <-- stack_ptr before pop     CCCC
 *               BBBB                               BBBB <--- stack_ptr after pop returns (CCCC)
 *               AAAA                               AAAA
 * --------------------------------------------------------------------------------
 */
GapStoryBoard *
gap_stb_undo_pop(GapStbTabWidgets *tabw)
{
  GapStoryBoard       *stb;

  stb = NULL;
  if(tabw == NULL)
  {
    return (NULL);
  }

  if (tabw->undo_stack_ptr == NULL)
  {
    return (NULL);
  }

  if (tabw->undo_stack_ptr == tabw->undo_stack_list)
  {
    /* at undo push the lastest available 
     */
    gap_stb_undo_push_clip(tabw, GAP_STB_FEATURE_LATEST, -1);
    
    /* advance stack_ptr 
     * (to compensate the extra push above)
     */
    tabw->undo_stack_ptr = tabw->undo_stack_ptr->next;
  }

  stb = gap_story_duplicate_full(tabw->undo_stack_ptr->stb);
  
  if(tabw->undo_stack_ptr->fileSnapshotBefore != NULL)
  {
    p_replace_file_from_snapshot(tabw->undo_stack_ptr->fileSnapshotBefore);
  }
  
  if(gap_debug)
  {
    printf("gap_stb_undo_pop returning feature_id:%d %s grp_count:%.2f next:%d\n"
      ,(int)tabw->undo_stack_ptr->feature_id
      ,gap_stb_undo_feature_to_string(tabw->undo_stack_ptr->feature_id)
      ,(float)tabw->undo_stack_group_counter
      ,(int)tabw->undo_stack_ptr->next
      );
    fflush(stdout);
  }
  
  tabw->undo_stack_ptr = tabw->undo_stack_ptr->next;

  gap_story_dlg_tabw_undo_redo_sensitivity(tabw);
  
  return (stb);
  
}  /* end gap_stb_undo_pop */


/* ---------------------------------------
 * gap_stb_undo_redo
 * ---------------------------------------
 * redo the feature that is above (e.g before) the undo stackpointer
 * returns a duplicate of the storyboard backup that is attached to the
 * element 2 above the undo stackpointer.
 * this refers to the backup version at the time BEFORE the
 * feature (that now shall be undone) was applied.
 *
 *           
 * stack_list -->latest                                  latest 
 *               EEEE                                    EEEE
 *               DDDD                                    DDDD <--- stack_ptr after redo (DDDD)
 *               CCCC  <-- stack_ptr before redo (DDDD)  CCCC        returns (EEEE)
 *               BBBB                                    BBBB 
 *               AAAA                                    AAAA
 * --------------------------------------------------------------------------------
 *
 * Example with one feature AAAA on the stack
 *
 * stack_ptr is NULL before redo(AAAA)           
 * stack_list -->latest                                  latest 
 *               AAAA                                    AAAA <--- stack_ptr after redo  AAAA
 *                                                                   returns (latest)
 * --------------------------------------------------------------------------------
 */
GapStoryBoard *
gap_stb_undo_redo(GapStbTabWidgets *tabw)
{
  GapStoryBoard       *stb;
  GapStoryUndoElem    *undo_elem;
  GapStoryUndoElem    *prev_elem;
  GapStoryUndoElem    *redo_elem;

  stb = NULL;
  if(tabw == NULL)
  {
    return (NULL);
  }

  prev_elem = NULL;
  redo_elem = NULL;
  for(undo_elem = tabw->undo_stack_list; undo_elem != NULL; undo_elem = undo_elem->next)
  {
    if(undo_elem->next == tabw->undo_stack_ptr)
    {
      redo_elem = prev_elem;
      tabw->undo_stack_ptr = undo_elem;
      break;
    }
    prev_elem = undo_elem;
  }

  if (redo_elem != NULL)
  {
    stb = gap_story_duplicate_full(redo_elem->stb);
    if(tabw->undo_stack_ptr != NULL)
    {
      if(tabw->undo_stack_ptr->fileSnapshotAfter != NULL)
      {
        p_replace_file_from_snapshot(tabw->undo_stack_ptr->fileSnapshotAfter);
      }
    }

    gap_story_dlg_tabw_undo_redo_sensitivity(tabw);

    if(gap_debug)
    {
      printf("gap_stb_undo_redo returning feature_id:%d %s grp_count:%.2f resulting stack_ptr:%d\n"
        ,(int)redo_elem->feature_id
        ,gap_stb_undo_feature_to_string(redo_elem->feature_id)
        ,(float)tabw->undo_stack_group_counter
        ,(int)tabw->undo_stack_ptr
        );
      fflush(stdout);
    }
  }
  
  return (stb);
  
}  /* end gap_stb_undo_redo */


/* ---------------------------------------
 * p_free_undo_elem
 * ---------------------------------------
 */
static void
p_free_undo_elem(GapStoryUndoElem    *undo_elem)
{
  if(undo_elem == NULL)
  {
    return;
  }

  if(gap_debug)
  {
    printf("p_free_undo_elem: %d %s"
          , (int)undo_elem
          , gap_stb_undo_feature_to_string(undo_elem->feature_id)
          );
    if(undo_elem->fileSnapshotBefore != NULL)
    {
      printf(" fileSnapshotBefore:%s filesize:%d mtime:%d"
        ,undo_elem->fileSnapshotBefore->filename
        ,(int)undo_elem->fileSnapshotBefore->filesize
        ,(int)undo_elem->fileSnapshotBefore->mtimefile
        );
    }
    if(undo_elem->fileSnapshotAfter != NULL)
    {
      printf(" fileSnapshotAfter:%s filesize:%d mtime:%d"
        ,undo_elem->fileSnapshotAfter->filename
        ,(int)undo_elem->fileSnapshotAfter->filesize
        ,(int)undo_elem->fileSnapshotAfter->mtimefile
        );
    }
    printf("\n");
    fflush(stdout);
  }

  
  if(undo_elem->stb)
  {
    gap_story_free_storyboard(&undo_elem->stb);
  }
  
  if(undo_elem->fileSnapshotBefore != NULL)
  {
    p_free_file_snapshot(undo_elem->fileSnapshotBefore);
    undo_elem->fileSnapshotBefore = NULL;
  }
  if(undo_elem->fileSnapshotAfter != NULL)
  {
    p_free_file_snapshot(undo_elem->fileSnapshotAfter);
    undo_elem->fileSnapshotAfter = NULL;
  }
  
  g_free(undo_elem);
  
}  /* end p_free_undo_elem */


/* ---------------------------------------
 * p_delete_redo_stack_area
 * ---------------------------------------
 * delete all undo elements beginning from stack_list root
 * up to exclusive stackpointer.
 * if the stackpointer points to an element with feature_id 
 *   GAP_STB_FEATURE_LATEST
 * then delete inclusive this element.
 *
 * after this procedure the stack_list root and stack_ptr
 * point to the same element (or both are NULL)
 */
static void
p_delete_redo_stack_area(GapStbTabWidgets *tabw)
{
  GapStoryUndoElem    *undo_elem;
  GapStoryUndoElem    *next_elem;
  GapStoryUndoElem    *new_root_elem;

  if(tabw == NULL)
  {
    return;
  }

  if(gap_debug)
  {
    printf("p_delete_redo_stack_area\n");
  }

  /* make sure that the stack_ptr does not point to an element
   * with feature_id GAP_STB_FEATURE_LATEST
   * (advance if necessary)
   */
  for(undo_elem = tabw->undo_stack_ptr; undo_elem != NULL; undo_elem = undo_elem->next)
  {
    if(undo_elem->feature_id != GAP_STB_FEATURE_LATEST)
    {
      break;
    }
    tabw->undo_stack_ptr = undo_elem->next;
  }
  
  new_root_elem = NULL;
  next_elem = NULL;
  for(undo_elem = tabw->undo_stack_list; undo_elem != NULL; undo_elem = next_elem)
  {
    if(undo_elem == tabw->undo_stack_ptr)
    {
      new_root_elem = undo_elem;
      break;
    }
    next_elem = undo_elem->next;
    p_free_undo_elem(undo_elem);
  }

  tabw->undo_stack_list = new_root_elem;
  return;
  
}  /* end p_delete_redo_stack_area */


/* ---------------------------------------
 * gap_stb_undo_destroy_undo_stack
 * ---------------------------------------
 * delete all undo elements beginning from stack_list root
 * up to exclusive stackpointer.
 * after this procedure the stack_list root and stack_ptr
 * point to the same element (or both are NULL)
 */
void
gap_stb_undo_destroy_undo_stack(GapStbTabWidgets *tabw)
{
  if(tabw == NULL)
  {
    return;
  }

  if(gap_debug)
  {
    printf("gap_stb_undo_destroy_undo_stack\n");
    fflush(stdout);
  }

  tabw->undo_stack_ptr = NULL;
  p_delete_redo_stack_area(tabw);

  gap_story_dlg_tabw_undo_redo_sensitivity(tabw);
  
}  /* end gap_stb_undo_destroy_undo_stack */


/* -----------------------------------------
 * gap_stb_undo_push_clip_with_file_snapshot
 * -----------------------------------------
 * create a new undo element (according to specified parameters)
 * and place it at top (first) of the undo stack.
 * if the stackpointer is NOT equal to the stack_list root,
 * then delete all undo elements from stack_list up to stackpointer.
 *
 * move the stackpointer to next element. (keep the element
 * on the stack list for undo purpose)
 *
 * if *filenamePtr points to an existing readable file, this procedure 
 * attaches a copy of the filename and its full content to the pushed element.
 * (fileSnapshotBefore)
 * Note that the current implementation keeps the filecontent in memory
 * because it is intended to hold small parameterfiles (xml settings for movepath transistion)
 * The filename Must NOT be used to store large data (as image or movie content)
 *
 * If the element at stack_ptr (that represents the previous procesing step)
 * has a fileSnapshot attached then this is also recorded as fileSnapshotAfter
 * 
 * Example
 * =======
 *  push element EEEE,  *filenamePtr points to "a.xml"
 *           
 * stack_list -->DDDD                               
 *               CCCC                                          EEEE <--- stack_ptr after push (EEEE, a.xml)
 *               BBBB  <-- stack_ptr before push(EEEE, a.xml)  BBBB      (deletes CCCC, DDDD)
 *               AAAA                                          AAAA
 * --------------------------------------------------------------------------------
 *
 * content of elem EEEE after the push EEEE operation:
 *
 *  EEEE.fileSnapshotBefore holds filename and content of a.xml (before processing of step EEEE)
 *  EEEE.fileSnapshotAfter is NULL
 *  EEEE.filenamePtr holds addr of the pointer that points to filename "a.xml"
 *
 *
 * Example continued: 
 * ==================
 *
 *
 *                                                             FFFF <--- stack_ptr after push (FFFF, NULL)
 * stack_list -->EEEE  <-- stack_ptr before push(FFFF, NULL)   EEEE ..... updated fileSnapshotAfter                              
 *               BBBB                                          BBBB                           
 *               AAAA                                          AAAA
 * --------------------------------------------------------------------------------
 *
 * content of elem EEEE after the push FFFF operation:
 *
 *  EEEE.fileSnapshotBefore holds filename and content of a.xml
 *          (before processing of step EEEE, that is relevant for undo EEEE purpose)
 *  EEEE.fileSnapshotAfter holds filename and content at time of push FFFF 
 *          (e.g. after processing EEEE is finished, that is relevant for redo EEEE purpose)
 *  EEEE.filenamePtr is rest to NULL
 *
 */
void
gap_stb_undo_push_clip_with_file_snapshot(GapStbTabWidgets *tabw
   , GapStoryFeatureEnum feature_id, gint32 story_id
   , char **filenamePtr)
{
  GapStoryBoard       *stb;
  GapStoryUndoElem    *new_undo_elem;
  GapStoryUndoElem    *top_undo_elem;


  if(tabw == NULL)
  {
    return;
  }

  if(gap_debug)
  {
    printf("gap_stb_undo_push_clip feature_id:%d %s grp_count:%.2f\n"
      ,(int)feature_id
      ,gap_stb_undo_feature_to_string(feature_id)
      ,(float)tabw->undo_stack_group_counter
      );
    fflush(stdout);
  }


  if(tabw->undo_stack_group_counter > 1)
  {
    return;
  }

  if(tabw->undo_stack_group_counter == 1)
  {
    tabw->undo_stack_group_counter = 1.5;
  }

  p_delete_redo_stack_area(tabw);

  stb = gap_story_dlg_tabw_get_stb_ptr(tabw);

  if(story_id >= 0)
  {
    switch (feature_id)
    {
      case GAP_STB_FEATURE_PROPERTIES_CLIP:
      case GAP_STB_FEATURE_PROPERTIES_TRANSITION:
        if(tabw->undo_stack_ptr != NULL)
        {
          if((tabw->undo_stack_ptr->feature_id == feature_id)
          && (tabw->undo_stack_ptr->clip_story_id == story_id))
          {
            /* multiple push of the same feature on the same clip object
             * are supressed. this collects multiple property changes
             * in sequence into one single undo step.
             */
            return;
          }
        }
        break;
      default:
        break;
    }
  }

  new_undo_elem = g_new(GapStoryUndoElem, 1);
  new_undo_elem->clip_story_id = story_id;
  new_undo_elem->feature_id = feature_id;

  new_undo_elem->filenamePtr = NULL;
  new_undo_elem->fileSnapshotBefore = NULL;
  new_undo_elem->fileSnapshotAfter = NULL;
  if(filenamePtr != NULL)
  {
    char *filename;
    
    new_undo_elem->filenamePtr = filenamePtr;
    filename = *filenamePtr;
    if(filename != NULL)
    {
      new_undo_elem->fileSnapshotBefore = p_create_file_snapshot(filename);
    }
  }
  
  top_undo_elem = tabw->undo_stack_list;
  if (top_undo_elem != NULL)
  {
    if(top_undo_elem->filenamePtr != NULL)
    {
      char *filename;

      /* at this point the top_undo_elem represents the previous step
       * that just has been finished, since the next step (new_undo_elem)
       * is ready to be pushed on the stack, and top_undo_elem->filenamePtr != NULL
       * indicates that the previous step may have edited a file.
       * now record this fileSnapshotAfter for redo purpose
       * Note that both filename and content may have changed in the previous step
       * since recording the fileSnapshotBefore.
       */
      filename = *top_undo_elem->filenamePtr;
      if(filename != NULL)
      {
        top_undo_elem->fileSnapshotAfter = p_create_file_snapshot(filename);
      }
    }
    /* clear the filenamePtr to disable multiple recording of fileSnapshotAfter
     * Note that filenamePtr refers to a clip that may only exist until the next processing step
     * that already can delete the clip (this can also happen in undo processing
     * where the storyboard is replaced by a duplicate and the adress)
     */
    top_undo_elem->filenamePtr = NULL;
  }
  
  new_undo_elem->next = tabw->undo_stack_list;

  tabw->undo_stack_list = new_undo_elem;
  tabw->undo_stack_ptr = new_undo_elem;

  gap_story_dlg_update_edit_settings(stb, tabw);

  new_undo_elem->stb = gap_story_duplicate_full(stb);

  gap_story_dlg_tabw_undo_redo_sensitivity(tabw);
  
}  /* end gap_stb_undo_push_clip_with_file_snapshot */



/* ---------------------------------------
 * gap_stb_undo_push_clip
 * ---------------------------------------
 * create a new undo element (according to specified parameters)
 * and place it at top (first) of the undo stack.
 * if the stackpointer is NOT equal to the stack_list root,
 * the delete all undo elements from stack_list up to stackpointer.
 *
 * move the stackpointer to next element. (keep the element
 * on the stack for redo purpose)
 *           
 * stack_list -->DDDD                               
 *               CCCC                                   EEEE <--- stack_ptr after push (EEEE)
 *               BBBB  <-- stack_ptr before push(EEEE)  BBBB      (deletes CCCC, DDDD)
 *               AAAA                                   AAAA
 * --------------------------------------------------------------------------------
 */
void
gap_stb_undo_push_clip(GapStbTabWidgets *tabw, GapStoryFeatureEnum feature_id, gint32 story_id)
{
  gap_stb_undo_push_clip_with_file_snapshot(tabw, feature_id, story_id, NULL);

}  /* end gap_stb_undo_push_clip */


/* ---------------------------------------
 * gap_stb_undo_push
 * ---------------------------------------
 */
void
gap_stb_undo_push(GapStbTabWidgets *tabw, GapStoryFeatureEnum feature_id)
{
  gap_stb_undo_push_clip(tabw, feature_id, -1 /* story_id */);
}  /* end gap_stb_undo_push */



/* ---------------------------------------
 * gap_stb_undo_group_begin
 * ---------------------------------------
 * 
 */
void
gap_stb_undo_group_begin(GapStbTabWidgets *tabw)
{
  if(tabw == NULL)
  {
    return;
  }
  tabw->undo_stack_group_counter += 1.0;
}  /* end gap_stb_undo_group_begin */


/* ---------------------------------------
 * gap_stb_undo_group_end
 * ---------------------------------------
 */
void
gap_stb_undo_group_end(GapStbTabWidgets *tabw)
{
  if(tabw == NULL)
  {
    return;
  }
  tabw->undo_stack_group_counter -= 1.0;
  if (tabw->undo_stack_group_counter < 1.0)
  {
    tabw->undo_stack_group_counter = 0.0;
  }
  
}  /* end gap_stb_undo_group_end */


/* ---------------------------------------
 * gap_stb_undo_get_undo_feature
 * ---------------------------------------
 * return NULL if undo not available
 *        or feature name that is ready to be un-done
 *        (dont g_free the returned string)
 */
const char *
gap_stb_undo_get_undo_feature(GapStbTabWidgets *tabw)
{
  const char *feature_name;
  
  if (tabw == NULL)
  {
    return (NULL);
  }

  if (tabw->undo_stack_ptr == NULL)
  {
    return (NULL);
  }

  feature_name = gap_stb_undo_feature_to_string(tabw->undo_stack_ptr->feature_id);
  return (feature_name);

}  /* end gap_stb_undo_get_undo_feature */


/* ---------------------------------------
 * gap_stb_undo_get_redo_feature
 * ---------------------------------------
 * return NULL if redo not available
 *        or feature name that is ready to be re-done
 *        (dont g_free the returned string)
 */
const char *
gap_stb_undo_get_redo_feature(GapStbTabWidgets *tabw)
{
  const char *feature_name;
  GapStoryUndoElem    *undo_elem;
  GapStoryUndoElem    *redo_elem;
  
  if (tabw == NULL)
  {
    return (NULL);
  }

  redo_elem = NULL;
  for(undo_elem = tabw->undo_stack_list; undo_elem != NULL; undo_elem = undo_elem->next)
  {
    if(undo_elem->next == tabw->undo_stack_ptr)
    {
      if(undo_elem->feature_id == GAP_STB_FEATURE_LATEST)
      {
        return (NULL);
      }
      redo_elem = undo_elem;
      break;
    }
  }

  if (redo_elem != NULL)
  {
    feature_name = gap_stb_undo_feature_to_string(redo_elem->feature_id);
    return (feature_name);
  }

  return (NULL);

}  /* end gap_stb_undo_get_redo_feature */

/* ---------------------------------------
 * gap_stb_undo_stack_set_unsaved_changes
 * ---------------------------------------
 * set the unsaved_changes flag = TRUE in
 * all storyboard backups in the undo stack.
 * this is typicall called on save.
 * (because the unsaved_changes in the back ups
 * refers to the state at capture time and gets invalid
 * when the storyboard is saved to file).
 */
void
gap_stb_undo_stack_set_unsaved_changes(GapStbTabWidgets *tabw)
{
  GapStoryUndoElem    *undo_elem;
  for(undo_elem = tabw->undo_stack_list; undo_elem != NULL; undo_elem = undo_elem->next)
  {
    if(undo_elem->stb != NULL)
    {
      undo_elem->stb->unsaved_changes = TRUE;
    }
  }
}  /* end gap_stb_undo_stack_set_unsaved_changes */
