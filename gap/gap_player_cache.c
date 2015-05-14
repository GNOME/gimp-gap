/*  gap_player_cache.c
 *
 *  This module handles frame caching for GAP video playback
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
 * version 2.2.1;   2006/05/22  hof: created
 */

/* The player cache structure overview:
 * oldest frames are at end of list, most recent accessed at start.
 *
 * HashTable:
 *   +---------------+
 *   | key, elem_ptr |------------------------+
 *   +---------------+                        |
 *   | key, elem_ptr |------------+           |
 *   +---------------+            |           |
 *   | key, elem_ptr |            |           |
 *   +---------------+            |           |
 *   | key, elem_ptr |--+         |           |
 *   +---------------+  |         |           |
 *                      |         |           | +--------- end_elem
 *                      |         |           | |
 * List:                V         V           V V
 *                  +-----+     +-----+     +-----+
 * start_elem: ---->| next|---->| next|---->| next|-->NULL
 *                  |     |     |     |     |     |
 *          NULL<---|prev |<----|prev |<----|prev |       CacheElem
 *                  +-----+     +-----+     +-----+
 *                     |           |           |
 *                     | cdata     | cdata     | cdata
 *                     |           |           |
 *                     V           V           V
 *                  +-----+     +-----+     +-----+
 *                  |     |     |     |     |     |        CacheData
 *                  |     |     |     |     |     |
 *                  |     |     |     |     |     |
 *                  +-----+     +-----+     +-----+
 *
 *
 * Example of the cache behaviour: 
 * (The example assumes that the cache maximum size is configured
 *  to hold a maximal 5 frames)
 *
 *     
 * Operation     List       Remarks                                                   
 * ---------------------------------------------------------------------------------  
 * 
 * add(1)        1                                                                    
 * add(2)        2 1                                                                  
 * add(3)        3 2 1                                                                
 * add(4)        4 3 2 1                                                              
 * add(5)        5 4 3 2 1                                                            
 * add(A)        A 5 4 3 2  (delete (1) to free resources for adding A)               
 * add(B)        B A 5 4 3  (delete (2) to free resources for adding B)               
 * lookup(A)     A B 5 4 3  (FOUND, relink A as first element)
 * lookup(1)     A B 5 4 3  (NOT FOUND, no change in the list)
 * add(5)        5 A B 4 3  (relink 5 as first element, but ignore adding 5 a 2nd time)       
 *
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_libgapbase.h"
#include "gap_player_main.h"
#include "gap_player_dialog.h"


#include "gap-intl.h"

extern int gap_debug;  /* 1 == print debug infos , 0 dont print debug infos */

// local debug setting
//static gint gap_debug = 1;  /* 1 == print debug infos , 0 dont print debug infos */


typedef struct GapPlayerCacheElem {
  gchar                            *ckey;
  GapPlayerCacheData               *cdata;
  void                             *next;
  void                             *prev;
  } GapPlayerCacheElem;

typedef struct GapPlayerCacheAdmin {  /* nick: admin_ptr */
  GapPlayerCacheElem               *start_elem;   /* recently accessed element */
  GapPlayerCacheElem               *end_elem;     /* last list elem (access long ago) */
  gint32                           summary_bytesize;
  gint32                           configured_max_bytesize;
  GHashTable                      *pcache_elemref_hash;
  } GapPlayerCacheAdmin;



static GapPlayerCacheAdmin *global_pca_ptr = NULL;


static GapPlayerCacheAdmin* p_get_admin_ptr(void);
static void      p_debug_printf_cache_list(GapPlayerCacheAdmin *admin_ptr);
static void      p_debug_printf_cdata(GapPlayerCacheData *cdata);

static void      p_player_cache_shrink(GapPlayerCacheAdmin *admin_ptr, gint32 new_bytesize);
static void      p_player_cache_remove_oldest_frame(GapPlayerCacheAdmin *admin_ptr);
static void      p_free_elem(GapPlayerCacheElem  *delete_elem_ptr);
static void      p_player_cache_add_new_frame(GapPlayerCacheAdmin *admin_ptr
                     , GapPlayerCacheElem *new_elem_ptr);
static void      p_relink_as_first_elem(GapPlayerCacheAdmin *admin_ptr
                     , GapPlayerCacheElem *elem_ptr);
static GapPlayerCacheElem* p_new_elem(const gchar *ckey
                                     , GapPlayerCacheData *cdata);





/* ------------------------------
 * p_get_admin_ptr
 * ------------------------------
 */
static GapPlayerCacheAdmin*
p_get_admin_ptr(void)
{
  if (global_pca_ptr == NULL)
  {
    GapPlayerCacheAdmin *admin_ptr;

    global_pca_ptr = g_new(GapPlayerCacheAdmin, 1);
    admin_ptr = global_pca_ptr;
    admin_ptr->start_elem = NULL;
    admin_ptr->end_elem = NULL;
    admin_ptr->summary_bytesize = 0;
    admin_ptr->configured_max_bytesize = GAP_PLAYER_CACHE_DEFAULT_MAX_BYTESIZE;

    /* use NULL to skip destructor for the ckey
     * because the ckey is also part of the value
     * (and therefore freed via the value destuctor procedure p_free_elem)
     */
    admin_ptr->pcache_elemref_hash = g_hash_table_new_full(g_str_hash
                                                         , g_str_equal
                                                         , NULL
                                                         , (GDestroyNotify) p_free_elem
                                                         );
    if(gap_debug)
    {
      printf("p_get_admin_ptr: %d\n", (int)global_pca_ptr);
    }
  }
  return (global_pca_ptr);
}  /* end p_init_admin */

/* ------------------------------
 * p_debug_printf_cdata
 * ------------------------------
 */
static void
p_debug_printf_cdata(GapPlayerCacheData *cdata)
{
    printf("  cdata  compression: %d, size:%d, th_data:%d, width:%d, height:%d, bpp:%d\n"
             , (int)cdata->compression
             , (int)cdata->th_data_size
             , (int)cdata->th_data
             , (int)cdata->th_width
             , (int)cdata->th_height
             , (int)cdata->th_bpp
             );
}  /* end p_debug_printf_cdata */

/* ------------------------------
 * p_debug_printf_cache_list
 * ------------------------------
 */
static void
p_debug_printf_cache_list(GapPlayerCacheAdmin *admin_ptr)
{
  GapPlayerCacheElem *elem_ptr;
  GapPlayerCacheElem *prev_elem_ptr;
  gint ii;

  printf("\np_debug_printf_cache_list: START start_elem:%d, end_elem:%d\n"
    ,(int)admin_ptr->start_elem
    ,(int)admin_ptr->end_elem
    );

  ii = 0;
  prev_elem_ptr = NULL;
  elem_ptr = admin_ptr->start_elem;
  while(elem_ptr != NULL)
  {
    printf("elem_ptr[%03d]: %d ckey: <%s> prev:%d next:%d\n"
             , (int)ii
             , (int)elem_ptr
             , elem_ptr->ckey
             , (int)elem_ptr->prev
             , (int)elem_ptr->next
             );
    ii++;

    if (elem_ptr->next == prev_elem_ptr)
    {
      printf("**** internal ERROR cache list is not properly linked !\n");
      break;
    }
    
    prev_elem_ptr = elem_ptr;

    elem_ptr = (GapPlayerCacheElem *)elem_ptr->next;


    if(elem_ptr == admin_ptr->start_elem)
    {
      printf("** internal ERROR cache list is not properly linked !\n");
      break;
    }
  }

  printf("p_debug_printf_cache_list: END\n");


}  /* end p_debug_printf_cache_list */


/* ---------------------------------
 * gap_player_cache_set_max_bytesize
 * ---------------------------------
 */
void
gap_player_cache_set_max_bytesize(gint32 max_bytesize)
{
  GapPlayerCacheAdmin *admin_ptr;

  admin_ptr = p_get_admin_ptr();
  if(admin_ptr)
  {
    admin_ptr->configured_max_bytesize = max_bytesize;
    p_player_cache_shrink(admin_ptr, 0);

    if (gap_debug)
    {
      printf("gap_player_cache_set_max_bytesize: max_player_cache: %d bytes (approximative frames: %d)\n"
        , (int)max_bytesize
        , (int)max_bytesize/ GAP_PLAYER_CACHE_FRAME_SZIE
        );
    }
  }

}  /* end gap_player_cache_set_max_bytesize */


/* -----------------------------------------
 * gap_player_cache_get_max_bytesize
 * -----------------------------------------
 */
gint32
gap_player_cache_get_max_bytesize(void)
{
  GapPlayerCacheAdmin *admin_ptr;
  gint32 bytesize;

  bytesize = 0;
  admin_ptr = p_get_admin_ptr();
  if(admin_ptr)
  {
    bytesize = admin_ptr->configured_max_bytesize;
  }
  return (bytesize);
}  /* end gap_player_cache_get_max_bytesize */


/* -----------------------------------------
 * gap_player_cache_get_current_bytes_used
 * -----------------------------------------
 */
gint32
gap_player_cache_get_current_bytes_used(void)
{
  GapPlayerCacheAdmin *admin_ptr;
  gint32 bytesize;

  bytesize = 0;
  admin_ptr = p_get_admin_ptr();
  if(admin_ptr)
  {
    bytesize = admin_ptr->summary_bytesize;
  }
  return (bytesize);

}  /* end gap_player_cache_get_current_bytes_used */


/* ------------------------------------------
 * gap_player_cache_get_current_frames_cached
 * ------------------------------------------
 */
gint32
gap_player_cache_get_current_frames_cached(void)
{
  GapPlayerCacheAdmin *admin_ptr;
  gint32 elem_counter;

  elem_counter = 0;
  admin_ptr = p_get_admin_ptr();
  if(admin_ptr)
  {
    elem_counter = g_hash_table_size(admin_ptr->pcache_elemref_hash);
  }
  return (elem_counter);
}  /* end gap_player_cache_get_current_frames_cached */

/* ------------------------------------
 * gap_player_cache_get_gimprc_bytesize
 * ------------------------------------
 */
gint32
gap_player_cache_get_gimprc_bytesize(void)
{
  gint32 bytesize;
  gchar *value_string;

  value_string = gimp_gimprc_query("video_playback_cache");
  if(value_string)
  {
    char *ptr;
    
    bytesize = atol(value_string);
    for(ptr=value_string; *ptr != '\0'; ptr++)
    {
      if ((*ptr == 'M') || (*ptr == 'm'))
      {
        bytesize *= (1024 * 1024);
        break;
      }
      if ((*ptr == 'K') || (*ptr == 'k'))
      {
        bytesize *= 1024;
        break;
      }
    }
    
    if (bytesize < GAP_PLAYER_CACHE_FRAME_SZIE)
    {
      /* turns OFF the cache */
      bytesize = 0; 
    }
    
    g_free(value_string);
  }
  else
  {
     /* nothing configured in gimprc file
      * use cache with default size.
      */
     bytesize = GAP_PLAYER_CACHE_DEFAULT_MAX_BYTESIZE; 
  }

  return (bytesize);

}  /* end gap_player_cache_get_gimprc_bytesize */


/* ------------------------------------
 * gap_player_cache_set_gimprc_bytesize
 * ------------------------------------
 */
void
gap_player_cache_set_gimprc_bytesize(gint32 bytesize)
{
  gint32 kbsize;
  gint32 mbsize;
  gchar  *value_string;
  
  kbsize = bytesize / 1024;
  mbsize = kbsize / 1024;
  value_string = NULL;
  
  if ((kbsize * 1024) != mbsize)
  {
    /* store as kilobytes to keep precision */
    value_string = g_strdup_printf("%dK", kbsize);
  }
  else
  {
    /* store as megabytes (value was not truncated by integer division) */
    value_string = g_strdup_printf("%dM", mbsize);
  }

  gimp_gimprc_set("video_playback_cache", value_string);
  g_free(value_string);

}  /* end gap_player_cache_set_gimprc_bytesize */



/* ------------------------------
 * p_get_elem_size
 * ------------------------------
 */
static gint32
p_get_elem_size(GapPlayerCacheElem *elem_ptr)
{
  gint32 bytesize;

  bytesize = 0;

  if(elem_ptr)
  {
    if(elem_ptr->cdata)
    {
      bytesize = elem_ptr->cdata->th_data_size;
    }
    if(elem_ptr->ckey)
    {
      bytesize += strlen(elem_ptr->ckey);
    }
    bytesize += (sizeof(GapPlayerCacheElem) + sizeof(GapPlayerCacheData));
  }

  return(bytesize);

}  /* end p_get_elem_size */


/* ------------------------------
 * p_relink_as_first_elem
 * ------------------------------
 * PRECONDITION:
 *  elem_ptr MUST be already linked somewhere in the list.
 */
static void
p_relink_as_first_elem(GapPlayerCacheAdmin *admin_ptr
                     , GapPlayerCacheElem *elem_ptr)
{
   if (admin_ptr->start_elem != elem_ptr)
   {
     /* relink elem_ptr as first element in the list. */
     GapPlayerCacheElem *prev;
     GapPlayerCacheElem *next;

     prev = (GapPlayerCacheElem *)elem_ptr->prev;
     next = (GapPlayerCacheElem *)elem_ptr->next;


     if(prev)
     {
       prev->next = next;
     }

     if(next)
     {
       next->prev = prev;
     }

     admin_ptr->start_elem->prev = elem_ptr;
     elem_ptr->next = admin_ptr->start_elem;
     elem_ptr->prev = NULL;
     admin_ptr->start_elem = elem_ptr;
     if(elem_ptr == admin_ptr->end_elem)
     {
       admin_ptr->end_elem = prev;
     }

   }
}  /* end p_relink_as_first_elem */

/* ------------------------------
 * p_new_elem
 * ------------------------------
 */
static GapPlayerCacheElem*
p_new_elem(const gchar *ckey
         , GapPlayerCacheData *cdata)
{
  GapPlayerCacheElem *elem_ptr;

  elem_ptr = g_new ( GapPlayerCacheElem, 1 );
  elem_ptr->ckey = g_strdup(ckey);
  elem_ptr->cdata = cdata;
  elem_ptr->next = NULL;
  elem_ptr->prev = NULL;

  return (elem_ptr);
}  /* end p_new_elem */


/* ------------------------------
 * gap_player_cache_free_all
 * ------------------------------
 * free up the player cache. (all cached frames and internal datastructures)
 *
 */
void
gap_player_cache_free_all(void)
{
  GapPlayerCacheAdmin *admin_ptr;

  admin_ptr = p_get_admin_ptr();

  if(gap_debug)
  {
    printf("gap_player_cache_free_all: %d\n", (int)admin_ptr);
  }

  if(admin_ptr)
  {
    while(admin_ptr->start_elem)
    {
      p_player_cache_remove_oldest_frame(admin_ptr);
    }
    g_hash_table_destroy(admin_ptr->pcache_elemref_hash);

    global_pca_ptr = NULL;
    g_free(admin_ptr);
  }
}  /* end gap_player_cache_free_all */


/* ----------------------------------
 * p_player_cache_shrink
 * ----------------------------------
 * check if size is greater than configured maximum
 * and free up that much elements to shrink memory usage
 * until fit to configured maximum
 */
static void
p_player_cache_shrink(GapPlayerCacheAdmin *admin_ptr, gint32 new_bytesize)
{
  while(admin_ptr->start_elem != NULL)
  {
    // debug limit cache to 5 elements for testing
    // if(g_hash_table_size(admin_ptr->pcache_elemref_hash) < 5)
    if((admin_ptr->summary_bytesize + new_bytesize) < admin_ptr->configured_max_bytesize)
    {
      if(gap_debug)
      {
        printf("p_player_cache_shrink: SPACE OK %d\n", (int)admin_ptr->summary_bytesize);
      }
      break; /* we are below the limit, no need to delete frames */
    }
    p_player_cache_remove_oldest_frame(admin_ptr);
  }
}  /* end p_player_cache_shrink */


/* ----------------------------------
 * p_player_cache_remove_oldest_frame
 * ----------------------------------
 */
void
p_player_cache_remove_oldest_frame(GapPlayerCacheAdmin *admin_ptr)
{
  GapPlayerCacheElem  *delete_elem_ptr;

  delete_elem_ptr = admin_ptr->end_elem;
  if (delete_elem_ptr)
  {
     if(gap_debug)
     {
       printf("p_player_cache_remove_oldest_frame: delete_elem_ptr: %d ckey: <%s> \n"
             , (int)delete_elem_ptr
             ,delete_elem_ptr->ckey
             );
     }
     admin_ptr->summary_bytesize -= p_get_elem_size(delete_elem_ptr);

     /* unlink from the list */
     admin_ptr->end_elem = delete_elem_ptr->prev;
     if (admin_ptr->end_elem)
     {
       admin_ptr->end_elem->next = NULL;
     }
     else
     {
       /* there is no previous element any more,
        * have to reset start pointer
        */
       admin_ptr->start_elem = NULL;
     }

     /* remove the reference from the hash table
      * (this also calls destructors to free the deleted list element too)
      */
     g_hash_table_remove(admin_ptr->pcache_elemref_hash, delete_elem_ptr->ckey);
  }
}  /* end p_player_cache_remove_oldest_frame */


/* ------------------------------
 * p_free_elem
 * ------------------------------
 */
static void
p_free_elem(GapPlayerCacheElem  *delete_elem_ptr)
{
  if(delete_elem_ptr == NULL)
  {
    return;
  }

  if(gap_debug)
  {
    printf("p_free_elem: delete_elem_ptr: %d, ckey: %d, cdata:%d"
      ,(int)delete_elem_ptr
      ,(int)delete_elem_ptr->ckey
      ,(int)delete_elem_ptr->cdata
      );
    if(delete_elem_ptr->ckey)
    {
      printf(" ckey_string <%s>"
        ,delete_elem_ptr->ckey
        );
    }
    printf("\n");
  }

  g_free(delete_elem_ptr->ckey);
  gap_player_cache_free_cdata(delete_elem_ptr->cdata);
  g_free(delete_elem_ptr);
}  /* end p_free_elem */


/* ------------------------------
 * p_player_cache_add_new_frame
 * ------------------------------
 * the new element is added as 1.st element to the cache list.
 * a reference to its address is stored in the hash table.
 * and the summary_bytesize is updated (increased).
 */
static void
p_player_cache_add_new_frame(GapPlayerCacheAdmin *admin_ptr
                     , GapPlayerCacheElem *new_elem_ptr)
{
  if(gap_debug)
  {
    printf("p_player_cache_add_new_frame START\n");
  }
  if(admin_ptr->start_elem)
  {
    admin_ptr->start_elem->prev = new_elem_ptr;
  }
  new_elem_ptr->next = admin_ptr->start_elem;
  new_elem_ptr->prev = NULL;

  admin_ptr->start_elem = new_elem_ptr;
  if(admin_ptr->end_elem == NULL)
  {
    admin_ptr->end_elem = new_elem_ptr;
  }

  admin_ptr->summary_bytesize += p_get_elem_size(new_elem_ptr);


  /* the adress of the newly added list element
   * is stored with the same ckey in a hash table.
   */
  g_hash_table_insert(admin_ptr->pcache_elemref_hash
                     ,new_elem_ptr->ckey
                     ,new_elem_ptr
                     );

  if(gap_debug)
  {
    guint elem_counter;
    if(1==0)
    {
      p_debug_printf_cache_list(admin_ptr);
    }
    printf("p_player_cache_add_new_frame END\n\n");
    
    
    elem_counter = g_hash_table_size(admin_ptr->pcache_elemref_hash);
    printf("\n # frames cached: %d  bytes_used:%d  max_bytesize:%d  percent:%03.2f\n"
       , (int)elem_counter
       , (int)admin_ptr->summary_bytesize
       , (int)admin_ptr->configured_max_bytesize
       , (float) 100.0 * ((float)admin_ptr->summary_bytesize 
                        / (float)MAX(1, admin_ptr->configured_max_bytesize))
       );

    printf("p_player_cache_add_new_frame END\n\n");
  }

}  /* end p_player_cache_add_new_frame */


/* ------------------------------
 * gap_player_cache_lookup
 * ------------------------------
 * search the players frame cache for the frame with
 * the specified ckey.
 * return the corresponding frame data struct if found
 * or NULL if not found.
 *
 * NOTE: the returned cdata is for read only use
 *         AND MUST NOT BE FREED !
 *       
 *       Typically the caller shall use the rturned cdata as input
 *       for the procedure gap_player_cache_decompress, to obtain
 *       the (uncompressed) workcopy of the frame data.
 */
GapPlayerCacheData*
gap_player_cache_lookup(const gchar *ckey)
{
  GapPlayerCacheElem *elem_ptr;
  GapPlayerCacheAdmin *admin_ptr;

  if(ckey == NULL)
  {
    return (NULL);
  }

  admin_ptr = p_get_admin_ptr();
  elem_ptr = (GapPlayerCacheElem *) g_hash_table_lookup (admin_ptr->pcache_elemref_hash,
                                                       ckey);
  if (elem_ptr != NULL)
  {
     p_relink_as_first_elem(admin_ptr, elem_ptr);
     if(gap_debug)
     {
       printf("gap_player_cache_lookup FOUND in player cache ckey:<%s>\n", ckey);
       //p_debug_printf_cdata(elem_ptr->cdata);
     }
     return (elem_ptr->cdata);
  }

  if(gap_debug)
  {
    printf("gap_player_cache_lookup NOT FOUND in player cache ckey:<%s>\n", ckey);
  }
  return (NULL);

}  /* end gap_player_cache_lookup */

/* ------------------------------
 * gap_player_cache_insert
 * ------------------------------
 * insert the specified frame ckey and data into the player frame cache.
 * NOTE: this procedure does lookup if the ckey is already in the cache
 *       and does not insert the same ckey more than once.
 * if the configured maximum chache limit is exceeded, one or more frames
 * in the cache are removed (to free up memory for the new frame).
 *
 * The current implementation removes (old) elements from the end of the
 * internal cache list, and adds new elements at the start of the list.
 * (read access re-links the accessed element as 1.st element)
 *
 */
void
gap_player_cache_insert(const gchar *ckey
                      , GapPlayerCacheData *cdata)
{
  GapPlayerCacheAdmin *admin_ptr;
  gint32               new_bytesize;
  GapPlayerCacheElem  *new_elem_ptr;

  if((ckey == NULL) || (cdata == NULL))
  {
    return;
  }
  
  if(gap_debug)
  {
    printf("gap_player_cache_insert: ckey:<%s>\n", ckey);
    p_debug_printf_cdata(cdata);
  }

  if(gap_player_cache_lookup(ckey) != NULL)
  {
    if(gap_debug)
    {
      printf("gap_player_cache_insert: INSERT REJECTED! ckey:<%s> \n", ckey);
    }
    return;
  }

  new_elem_ptr = p_new_elem(ckey, cdata);
  new_bytesize = p_get_elem_size(new_elem_ptr);
  admin_ptr = p_get_admin_ptr();

  p_player_cache_shrink(admin_ptr, new_bytesize);
  p_player_cache_add_new_frame(admin_ptr, new_elem_ptr);

}  /* end gap_player_cache_insert */



/* ------------------------------
 * gap_player_cache_decompress
 * ------------------------------
 * return the decompressed RGB buffer (th_data)
 * (for uncompressed frames this is just a copy of the chaced data)
 * The caller is responsible to g_free the returned data after use.
 */
guchar*
gap_player_cache_decompress(GapPlayerCacheData *cdata)
{
  guchar *th_data;

  th_data = NULL;
  if (cdata->compression == GAP_PLAYER_CACHE_COMPRESSION_NONE)
  {
    th_data = g_new ( guchar, cdata->th_data_size );
    memcpy(th_data, cdata->th_data, cdata->th_data_size);
    if(gap_debug)
    {
      printf("gap_player_cache_decompress: th_data:%d size:%d, cdata->th_data: %d\n"
        , (int)th_data
        , (int)cdata->th_data_size
        , (int)cdata->th_data
        );
    }
  }
  else
  {
    printf("** ERROR: player chache compression not implemented yet.\n");
  }

  return (th_data);

}  /* end gap_player_cache_decompress */


/* ------------------------------
 * gap_player_cache_new_data
 * ------------------------------
 * create and set up a new player chache data stucture.
 * NOTE: The th_data is NOT copied but used as 1:1 reference
 *       in case no compression is done.
 * comression (not implemented yet) will create
 * a compressed copy of th_data (that is put into the newly created
 * GapPlayerCacheData structure, and g_free th_data after the compression.
 */
GapPlayerCacheData*
gap_player_cache_new_data(guchar *th_data
                         , gint32 th_size
                         , gint32 th_width
                         , gint32 th_height
                         , gint32 th_bpp
                         , GapPlayerCacheCompressionType compression
                         , gint32 flip_status
                         )
{
  GapPlayerCacheData* cdata;

  cdata = g_new ( GapPlayerCacheData, 1 );
  cdata->compression = compression;
  cdata->th_data_size = th_size;
  cdata->th_width = th_width;
  cdata->th_height = th_height;
  cdata->th_bpp = th_bpp;
  cdata->flip_status = flip_status;
  if (compression == GAP_PLAYER_CACHE_COMPRESSION_NONE)
  {
    cdata->th_data = th_data;

  }
  else
  {
    printf("** ERROR: player chache compression not implemented yet.\n");
    cdata->compression = GAP_PLAYER_CACHE_COMPRESSION_NONE;
    cdata->th_data = th_data;
  }

  return (cdata);
}  /* end gap_player_cache_new_data */



/* ------------------------------
 * gap_player_cache_free_cdata
 * ------------------------------
 */
void
gap_player_cache_free_cdata(GapPlayerCacheData *cdata)
{
  if(cdata)
  {
    if(cdata->th_data)
    {
      g_free(cdata->th_data);
    }
    g_free(cdata);
  }
}  /* end gap_player_cache_free_cdata */



/* ------------------------------
 * gap_player_cache_new_movie_key
 * ------------------------------
 */
gchar *
gap_player_cache_new_movie_key(const char *filename
                         , gint32  framenr
                         , gint32  seltrack
                         , gdouble delace
                         )
{
  char *ckey;
  char *abs_filename;

  abs_filename = gap_file_build_absolute_filename(filename);
  ckey = g_strdup_printf("[@MOVIE]:%d:%s:%06d:%d:%1.3f"
               , (int)gap_file_get_mtime(filename)
               , abs_filename
               , (int)framenr
               , (int)seltrack
               , (float)delace
               );
  g_free(abs_filename);

  return (ckey);
}  /* end gap_player_cache_new_movie_key */


/* ------------------------------
 * gap_player_cache_new_image_key
 * ------------------------------
 */
gchar*
gap_player_cache_new_image_key(const char *filename)
{
  char *ckey;
  char *abs_filename;

  abs_filename = gap_file_build_absolute_filename(filename);
  ckey = g_strdup_printf("[@IMAGE]:%d:%s"
               , (int)gap_file_get_mtime(filename)
               , abs_filename
               );
  g_free(abs_filename);

  return (ckey);
}  /* end gap_player_cache_new_image_key */



/* ------------------------------
 * gap_player_cache_new_composite_video_key
 * ------------------------------
 * type:    use 0 when playback the full full storyboard, 1 for partial playback
 * version: internal counter (increased each time the storyboard is changed in the edit dialog)
 */
gchar*
gap_player_cache_new_composite_video_key(const char *filename
  , gint32 framenr
  , gint32 type
  , gint32 version
  )
{
  char *ckey;
  char *abs_filename;

  abs_filename = gap_file_build_absolute_filename(filename);
  ckey = g_strdup_printf("[@COMPOSITE]:%06d:%d:%04d:%s"
               , (int)framenr
               , (int)type
               , (int)version
               , abs_filename
               );
  g_free(abs_filename);

  return (ckey);
}  /* end gap_player_cache_new_composite_video_key */

