/* gap_frame_fetcher.c
 *
 *
 *  The FrameFetcher provides access to frames both from imagefiles and videofiles.
 *  
 *  It holds a global image cache of temporary gimp images intended for
 *  read only access in various gimp-gap render processings.
 *  (those cached images are marked with an image parasite)
 *  
 *  There are methods to get the temporary image 
 *  or to get a duplicate that has only one layer at imagesize.
 *  (merged or picked via desired stackposition)
 *
 *  For videofiles it holds a cache of open videofile handles.
 *  (note that caching of videoframes is already available in the videohandle)
 *  
 *  The current implementation of the frame fetcher is NOT multithread save !
 *  (the procedures may drop cached images that are still in use by a concurrent thread
 *  further the cache lists can be messed up if they are modified by concurrent threads
 *  at the same time.
 *
 *  Currently there is no support to keep track of cached images during the full length
 *  of a gimp session. Therefore unregister of the last user does NOT drop all resources
 *  
 *  (If there are still registrated users at exit time, the cached images are still
 *  loaded in the gimp session)
 *
 *  TODO: user registration shall be serialized and stored via gimp_set_data
 *        to keep track over the entire gimp session.
 *

 *
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
 * 2008.08.20  hof  - created (moved image cache stuff from gap_story_render_processing modules to this  new module)
 *                  - new feature:  caching of videohandles.
 *
 */

#include <config.h>

/* SYTEM (UNIX) includes */
#include <stdlib.h>
//#include <sys/types.h>
//#include <sys/stat.h>
//#include <math.h>
//#include <errno.h>

//#include <dirent.h>


#ifdef GAP_HAVE_SYSINFO
#include <sys/sysinfo.h>
#endif



#include <glib/gstdio.h>



/* GIMP includes */
#include "gtk/gtk.h"
#include "gap-intl.h"
#include "libgimp/gimp.h"


#include "gap_libgapbase.h"
#include "gap_libgimpgap.h"
#include "gap_lib_common_defs.h"
#include "gap_layer_copy.h"
//#include "gap_fmac_name.h"

#include "gap_frame_fetcher.h"

#define GAP_FFETCH_MAX_IMG_CACHE_ELEMENTS 18
#define GAP_FFETCH_MAX_GVC_CACHE_ELEMENTS 6
#define GAP_FFETCH_GVA_FRAMES_TO_KEEP_CACHED 36

#define GAP_FFETCH_MAX_IMG_CACHE_ELEMENTS_GIMPRC_KEY     "gap_ffetch_max_img_cache_elements"
#define GAP_FFETCH_MAX_GVC_CACHE_ELEMENTS_GIMPRC_KEY     "gap_ffetch_max_gvc_cache_elements" 
#define GAP_FFETCH_GVA_FRAMES_TO_KEEP_CACHED_GIMPRC_KEY  "gap_ffetch_gva_frames_to_keep_cached"


/* the lists of cached images and duplicates are implemented via GIMP image parasites,
 * where images are simply loaded by GIMP without adding a display and marked with a non persistent parasite.
 * the GAP_IMAGE_CACHE_PARASITE 
 *     holds the modification timestamp (mtime), gint32 ffetch_user_id
 *     gint32 type (GAP_IMAGE_CACHE_TYPE_ORIGINAL_SIZE or GAP_IMAGE_CACHE_TYPE_PRESCALE_ENABLED)
 *     gint32 orig_width, gint32 orig_height (the unscaled original size of the image)
 *     and full filename (inclusive terminating 0)
 * the GAP_IMAGE_DUP_CACHE_PARASITE holds the gint32 ffetch_user_id
 */

#define GAP_IMAGE_CACHE_PARASITE "GAP-IMAGE-CACHE-PARASITE"
#define GAP_IMAGE_DUP_CACHE_PARASITE "GAP-IMAGE-DUP-CACHE-PARASITE"

#define GAP_IMAGE_CACHE_TYPE_ORIGINAL_SIZE    0
#define GAP_IMAGE_CACHE_TYPE_PRESCALE_ENABLED 1

typedef struct GapImageCacheParasitePointers {
  gint32 *mtime_ptr;
  gint32 *ffetch_id_ptr;
  gint32 *type_ptr;
  gint32 *orig_width_ptr;
  gint32 *orig_height_ptr;
  gchar  *filename_ptr;
} GapImageCacheParasitePointers;


typedef struct GapFFetchResourceUserElem
{
   gint32  ffetch_user_id;
   void *next;
} GapFFetchResourceUserElem;

#define MAX_OLD_IMAGE_IDS 10

typedef struct GapImageChacheInfo  /* nickname: cinf */
{
   gint32 old_cached_image_ids[MAX_OLD_IMAGE_IDS];
   gint32 number_of_old_cached_images;
   gint32 number_of_cached_images;
} GapImageChacheInfo;


/* -------- types for the video handle cache  ------- */

typedef struct GapFFetchGvahandCacheElem
{
   t_GVA_Handle *gvahand;
   gint32        mtime;
   gint32        seltrack;
   char         *filename;
   void *next;
} GapFFetchGvahandCacheElem;


typedef struct GapFFetchGvahandCache
{
  GapFFetchGvahandCacheElem *gvc_list;
  gint32            max_vid_cache;  /* number of videohandles to hold in the cache */
} GapFFetchGvahandCache;



extern int gap_debug;  /* 1 == print debug infos , 0 dont print debug infos */

/*************************************************************
 *         STATIC varaibles                                  *
 *************************************************************
 */

static GapFFetchGvahandCache *global_gvcache = NULL;

static GapFFetchResourceUserElem *global_rsource_users = NULL;


/*************************************************************
 *         FRAME FETCHER procedures                          *
 *************************************************************
 */
static void          p_init_GapImageCacheParasitePointers(GapImageCacheParasitePointers *paraPtr
                        , guchar *parasite_data
                        );
static gint32        p_find_cache_image(const char* filename, gint32 ffetch_user_id
                         , gint32 cachedImageType
                         , GapImageChacheInfo *cinf
                         , gint32 *originalWidthPtr, gint32 *originalHeightPtr
                         );
static gint32        p_load_image_and_add_to_cache(const char* filename, gint32 ffetch_user_id
                         , gboolean addToCache, gint32 cachedImageType
                         , GapImageChacheInfo *cinf
                         , gint32 *originalWidthPtr, gint32 *originalHeightPtr
                         );

static gint32         p_load_cache_image(const char* filename, gint32 ffetch_user_id, gboolean addToCache);
static void           p_drop_image_cache(void);
#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
static void           p_drop_gvahand_cache_elem1(GapFFetchGvahandCache *gvcache);
static void           p_drop_vidhandle_cache(void);

static t_GVA_Handle*  p_ffetch_get_open_gvahand(const char* filename, gint32 seltrack
                          , const char *preferred_decoder);
#endif

static void           p_add_image_to_list_of_duplicated_images(gint32 image_id, gint32 ffetch_user_id);

static gint32 p_get_ffetch_max_img_cache_elements();
static gint32 p_get_ffetch_max_gvc_cache_elements();
static gint32 ffetch_gva_frames_to_keep_cached();

/* ----------------------------------------------------
 * p_init_GapImageCacheParasitePointers
 * ----------------------------------------------------
 * init pointers in the GapImageCacheParasitePointers to access the
 * parasite data.
 * Note that the filename_ptr refers to the last entry that has variable size
 * and is a \0 terminated string.
 */
static void
p_init_GapImageCacheParasitePointers(GapImageCacheParasitePointers *paraPtr, guchar *parasite_data)
{
  paraPtr->mtime_ptr =       (gint32 *)parasite_data;
  paraPtr->ffetch_id_ptr =   (gint32 *)&parasite_data[sizeof(gint32)];
  paraPtr->type_ptr =        (gint32 *)&parasite_data[2 * sizeof(gint32)];
  paraPtr->orig_width_ptr =  (gint32 *)&parasite_data[3 * sizeof(gint32)];
  paraPtr->orig_height_ptr = (gint32 *)&parasite_data[4 * sizeof(gint32)];
  paraPtr->filename_ptr =    (gchar *) &parasite_data[5 * sizeof(gint32)];

}  /* end p_init_GapImageCacheParasitePointers */

/* ----------------------------------------------------
 * p_get_ffetch_max_img_cache_elements
 * ----------------------------------------------------
 */
static gint32
p_get_ffetch_max_img_cache_elements()
{
  static gint32 value = -1;
  static char *gimprc_key = GAP_FFETCH_MAX_IMG_CACHE_ELEMENTS_GIMPRC_KEY;
  
  if(value == -1)
  {
    if(gap_debug)
    {
      printf("get gimprc value for %s\n"
        , gimprc_key
        );
    }
    value = gap_base_get_gimprc_int_value (gimprc_key
        , GAP_FFETCH_MAX_IMG_CACHE_ELEMENTS /* default_value*/
        , 1                                 /* min_value */
        , 2000                              /* max_value */
        );
  }
  if(gap_debug)
  {
    printf("value for %s is:%d\n"
        , gimprc_key
        ,(int)value
        );
  }
  return (value);
}  /* end p_get_ffetch_max_img_cache_elements */

/* ----------------------------------------------------
 * p_get_ffetch_max_gvc_cache_elements
 * ----------------------------------------------------
 */
static gint32
p_get_ffetch_max_gvc_cache_elements()
{
  static gint32 value = -1;
  static char *gimprc_key = GAP_FFETCH_MAX_GVC_CACHE_ELEMENTS_GIMPRC_KEY;
  
  if(value == -1)
  {
    if(gap_debug)
    {
      printf("get gimprc value for %s\n"
        , gimprc_key
        );
    }
    value = gap_base_get_gimprc_int_value (gimprc_key
        , GAP_FFETCH_MAX_GVC_CACHE_ELEMENTS /* default_value*/
        , 1                                 /* min_value */
        , 80                              /* max_value */
        );
  }
  if(gap_debug)
  {
    printf("value for %s is:%d\n"
        , gimprc_key
        ,(int)value
        );
  }
  return (value);
}  /* end p_get_ffetch_max_gvc_cache_elements */

/* ----------------------------------------------------
 * p_get_ffetch_gva_frames_to_keep_cached
 * ----------------------------------------------------
 */
static gint32
p_get_ffetch_gva_frames_to_keep_cached()
{
  static gint32 value = -1;
  static char *gimprc_key = GAP_FFETCH_GVA_FRAMES_TO_KEEP_CACHED_GIMPRC_KEY;
  
  if(value == -1)
  {
    if(gap_debug)
    {
      printf("get gimprc value for %s\n"
        , gimprc_key
        );
    }
    value = gap_base_get_gimprc_int_value (gimprc_key
        , GAP_FFETCH_GVA_FRAMES_TO_KEEP_CACHED /* default_value*/
        , 1                                 /* min_value */
        , 1000                              /* max_value */
        );
  }
  if(gap_debug)
  {
    printf("value for %s is:%d\n"
        , gimprc_key
        ,(int)value
        );
  }
  return (value);
}  /* end p_get_ffetch_gva_frames_to_keep_cached */


/* ----------------------------------------------------
 * p_find_cache_image
 * ----------------------------------------------------
 * find an image with filename and matching cachedImageType 
 *   GAP_IMAGE_CACHE_TYPE_ORIGINAL_SIZE or
 *   GAP_IMAGE_CACHE_TYPE_PRESCALE_ENABLED
 * in the cache.
 * returns the imageId of the cached image or -1 if not found in the cache.
 *
 * OUT parameters
 *   first_cached_image_id    .. id of the 1st image in the cache.
 *   number_of_cached_images  .. total number of cached images (only valid when image was NOT FOUND!)
 *   originalWidth[Height]Ptr .. the unscaled original size of the image.
 */
static gint32
p_find_cache_image(const char* filename, gint32 ffetch_user_id
   , gint32 cachedImageType
   ,  GapImageChacheInfo *cinf
   , gint32 *originalWidthPtr, gint32 *originalHeightPtr
   )
{
  gint32 l_image_id;

  gint32 *images;
  gint    nimages;
  gint    l_idi;
  GimpParasite  *l_parasite;

  cinf->old_cached_image_ids[0] = -1;
  cinf->number_of_old_cached_images = 0;
  cinf->number_of_cached_images = 0;
  *originalWidthPtr = 0;
  *originalHeightPtr = 0;
  if(gap_debug)
  {
    printf("p_find_cache_image START cachedImageType:%d %s\n"
      ,(int)cachedImageType
      ,filename
      );
  }
  if(filename == NULL)
  {
    printf("p_find_cache_image: ** ERROR cant load filename == NULL! pid:%d\n", (int)gap_base_getpid());
    return -1;
  }

  l_image_id = -1;
  images = gimp_image_list(&nimages);
  for(l_idi=0; l_idi < nimages; l_idi++)
  {
    l_parasite = gimp_image_parasite_find(images[l_idi], GAP_IMAGE_CACHE_PARASITE);

    if(l_parasite)
    {
      GapImageCacheParasitePointers para;
      GapImageCacheParasitePointers *paraPtr;
      
      paraPtr = &para;
      p_init_GapImageCacheParasitePointers(paraPtr, l_parasite->data);

    
      cinf->number_of_cached_images++;
      if (cinf->number_of_old_cached_images < MAX_OLD_IMAGE_IDS)
      {
        cinf->old_cached_image_ids[cinf->number_of_old_cached_images] = images[l_idi];
        cinf->number_of_old_cached_images++;
      }
      
      if ((*(paraPtr->type_ptr) == cachedImageType)
      && (strcmp(filename, paraPtr->filename_ptr) == 0))
      {
        gint32 mtimefile;
        
        mtimefile = gap_file_get_mtime(filename);
        if(mtimefile == *(paraPtr->mtime_ptr))
        {
          /* image found in cache */
          l_image_id = images[l_idi];
          *originalWidthPtr = *(paraPtr->orig_width_ptr);
          *originalHeightPtr = *(paraPtr->orig_height_ptr);
        }
        else
        {
          /* image found in cache, but has changed modification timestamp
           * (delete from cache and reload)
           */
          if(gap_debug)
          {
            printf("FrameFetcher: DELETE because mtime changed : (image_id:%d) ffetchId:%d name:%s  mtimefile:%d mtimecache:%d  pid:%d\n"
                  , (int)images[l_idi]
                  , (int)*(paraPtr->ffetch_id_ptr)
                  , gimp_image_get_filename(images[l_idi])
                  , (int)mtimefile
                  , (int)*(paraPtr->mtime_ptr)
                  , (int)gap_base_getpid()
                  );
          }
          gap_image_delete_immediate(images[l_idi]);
        }
        l_idi = nimages -1;  /* force break at next loop iteration */
      }
      gimp_parasite_free(l_parasite);
    }
  }
  if(images)
  {
    g_free(images);
  }
  
  
  if (l_image_id >= 0)
  {
    if(gap_debug)
    {
      printf("FrameFetcher: p_find_cache_image CACHE-HIT :%s (image_id:%d) pid:%d\n"
            , filename, (int)l_image_id, (int)gap_base_getpid());
    }
    return(l_image_id);
  }

  if(gap_debug)
  {
    printf("p_find_cache_image END Not found in cache %s\n"
      ,filename
      );
  }
  
  return (-1);  /* indicates image not found in cache */

}  /* end p_find_cache_image */




/* ----------------------------------------------------
 * p_load_image_and_add_to_cache
 * ----------------------------------------------------
 * load the image from file (depends on enableLoad flag)
 *
 * in case the flag addToCache is TRUE the loaded image will be automatically added
 * to the cache after read from file operation.
 * This will also delete the the first cached imge in case the number_of_cached_images
 * exceded the configured limit (gimprc paramter "gap_ffetch_max_img_cache_elements")
 */
static gint32
p_load_image_and_add_to_cache(const char* filename, gint32 ffetch_user_id
   , gboolean addToCache, gint32 cachedImageType
   , GapImageChacheInfo *cinf
   , gint32 *originalWidthPtr, gint32 *originalHeightPtr
   )
{
  gint32 l_image_id;
  char *l_filename;

  GimpParasite  *l_parasite;

  if(filename == NULL)
  {
    printf("p_load_image_and_add_to_cache: ** ERROR cant load filename == NULL! pid:%d\n", (int)gap_base_getpid());
    return -1;
  }

  l_filename = g_strdup(filename);
  l_image_id = gap_lib_load_image(l_filename);
  if(gap_debug)
  {
    printf("FrameFetcher: loaded image from disk:%s (image_id:%d) pid:%d\n"
      , l_filename, (int)l_image_id, (int)gap_base_getpid());
  }

  if(l_image_id >= 0)
  {
    *originalWidthPtr = gimp_image_width(l_image_id);
    *originalHeightPtr = gimp_image_height(l_image_id);
    
    if(addToCache == TRUE)
    {
      guchar *parasite_data;
      gint32  parasite_size;
      gint32  len_filename0;           /* filename length including the terminating 0 */
      GapImageCacheParasitePointers para;
      GapImageCacheParasitePointers *paraPtr;
      gint    ii;
      
      if(gap_debug)
      {
         printf("(L2) number_of_cached_images:%d first_cached_image_id:%d\n"
            , (int)cinf->number_of_cached_images
            , (int)cinf->old_cached_image_ids[0]
            );
      }
      
      ii = 0;
      while ((cinf->number_of_cached_images >= p_get_ffetch_max_img_cache_elements())
      && (cinf->old_cached_image_ids[ii] >= 0))
      {
        /* the image cache already has more elements than desired,
         * drop the 1st cached image
         */
        if(gap_debug)
        {
          printf("FrameFetcher: DELETE because cache is full: (image_id:%d)  name:%s number_of_cached_images:%d pid:%d\n"
                , (int)cinf->old_cached_image_ids[ii]
                , gimp_image_get_filename(cinf->old_cached_image_ids[ii])
                , (int)cinf->number_of_cached_images
                , (int)gap_base_getpid()
                );
        }
        gap_image_delete_immediate(cinf->old_cached_image_ids[ii]);
        cinf->number_of_cached_images--;
        ii++;
        if((ii >= MAX_OLD_IMAGE_IDS)
        || (ii >= cinf->number_of_old_cached_images))
        {
          break;
        }
      }
    
      /* build parasite data including mtime and full filename with terminating 0 byte */
      len_filename0 = strlen(filename) + 1;
      parasite_size = 5 * sizeof(gint32) + len_filename0;  
      parasite_data = g_malloc0(parasite_size);

      paraPtr = &para;
      p_init_GapImageCacheParasitePointers(paraPtr, parasite_data);

      *(paraPtr->mtime_ptr) = gap_file_get_mtime(filename);
      *(paraPtr->ffetch_id_ptr) = ffetch_user_id;
      *(paraPtr->type_ptr) = cachedImageType;
      *(paraPtr->orig_width_ptr) = gimp_image_width(l_image_id);
      *(paraPtr->orig_height_ptr) = gimp_image_height(l_image_id);
      memcpy(paraPtr->filename_ptr, filename, len_filename0);
      
      /* attach a parasite to mark the image as part of the gap image cache */
      l_parasite = gimp_parasite_new(GAP_IMAGE_CACHE_PARASITE
                                     ,0  /* GIMP_PARASITE_PERSISTENT  0 for non persistent */
                                     ,parasite_size
                                     ,parasite_data
                                     );
    
      if(l_parasite)
      {
        gimp_image_parasite_attach(l_image_id, l_parasite);
        gimp_parasite_free(l_parasite);
      }
      g_free(parasite_data);
    
    }
  
  }
  

  g_free(l_filename);

  return(l_image_id);
}  /* end p_load_image_and_add_to_cache */



/* ----------------------------------------------------
 * p_load_cache_image
 * ----------------------------------------------------
 * load an image from cache or from file (in case image is not already cached)
 * in case the flag addToCache is TRUE the image will be automatically added
 * to the cache after read from file operation.
 * Note: this procedure handles only images at original size (GAP_IMAGE_CACHE_TYPE_ORIGINAL_SIZE)
 * a prescaled cached variant of the image will be ignored (e.g NOT be found by this procedure)
 */
static gint32
p_load_cache_image(const char* filename, gint32 ffetch_user_id, gboolean addToCache)
{
  gint32 retImgageId;
  gint32 originalWidth;
  gint32 originalHeight;
  GapImageChacheInfo  gapImageChacheInfo;
  if(gap_debug)
  {
    printf("p_load_cache_image START %s\n"
      ,filename
      );
  }
  retImgageId = p_find_cache_image(filename, ffetch_user_id
     , GAP_IMAGE_CACHE_TYPE_ORIGINAL_SIZE  /* cachedImageType */
     , &gapImageChacheInfo
     , &originalWidth, &originalHeight
     );
  if (retImgageId < 0)
  {
    retImgageId = p_load_image_and_add_to_cache(filename, ffetch_user_id
                           , addToCache
                           , GAP_IMAGE_CACHE_TYPE_ORIGINAL_SIZE  /* cachedImageType */
                           , &gapImageChacheInfo
                           , &originalWidth
                           , &originalHeight
                         );
  }

  if(gap_debug)
  {
    printf("p_load_cache_image END id:%d %s\n"
      ,(int)retImgageId
      , filename
      );
  }
  
  return (retImgageId);
  
}   /* end p_load_cache_image */



/* ----------------------------------------------------
 * p_drop_image_cache
 * ----------------------------------------------------
 * drop the image cache.
 */
static void
p_drop_image_cache(void)
{
  gint32 *images;
  gint    nimages;
  gint    l_idi;
  
  if(gap_debug)
  {
    printf("p_drop_image_cache START pid:%d\n", (int) gap_base_getpid());
  }

  images = gimp_image_list(&nimages);
  for(l_idi=0; l_idi < nimages; l_idi++)
  {
    GimpParasite  *l_parasite;
  
    l_parasite = gimp_image_parasite_find(images[l_idi], GAP_IMAGE_CACHE_PARASITE);

    if(gap_debug)
    {
      printf("FrameFetcher: CHECK (image_id:%d) name:%s pid:%d\n"
            , (int)images[l_idi]
            , gimp_image_get_filename(images[l_idi])
            , (int)gap_base_getpid()
            );
    }

    if(l_parasite)
    {
      if(gap_debug)
      {
        printf("FrameFetcher: DELETE (image_id:%d) name:%s pid:%d\n"
              , (int)images[l_idi]
              , gimp_image_get_filename(images[l_idi])
              , (int)gap_base_getpid()
              );
      }
      /* delete image from the duplicates cache */
      gap_image_delete_immediate(images[l_idi]);
      gimp_parasite_free(l_parasite);
    }
  }
  if(images)
  {
    g_free(images);
  }

  if(gap_debug)
  {
    printf("p_drop_image_cache END pid:%d\n", (int)gap_base_getpid());
  }

}  /* end p_drop_image_cache */



#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
/* ----------------------------------------------------
 * p_drop_gvahand_cache_elem1
 * ----------------------------------------------------
 */
static void
p_drop_gvahand_cache_elem1(GapFFetchGvahandCache *gvcache)
{
  GapFFetchGvahandCacheElem  *gvc_ptr;

  if(gvcache)
  {
    gvc_ptr = gvcache->gvc_list;
    if(gvc_ptr)
    {
      if(gap_debug)
      {
        printf("p_drop_gvahand_cache_elem1 delete:%s (gvahand:%d seltrack:%d mtime:%ld)\n"
               , gvc_ptr->filename, (int)gvc_ptr->gvahand
               , (int)gvc_ptr->seltrack, (long)gvc_ptr->mtime);
      }
      GVA_close(gvc_ptr->gvahand);
      g_free(gvc_ptr->filename);
      gvcache->gvc_list = (GapFFetchGvahandCacheElem  *)gvc_ptr->next;
      g_free(gvc_ptr);
    }
  }
}  /* end p_drop_gvahand_cache_elem1 */
#endif


#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
/* ----------------------------------------------------
 * p_drop_vidhandle_cache
 * ----------------------------------------------------
 */
static void
p_drop_vidhandle_cache(void)
{
  GapFFetchGvahandCache *gvc_cache;

  if(gap_debug)
  {
    printf("p_drop_vidhandle_cache START\n");
  }
  gvc_cache = global_gvcache;
  if(gvc_cache)
  {
    while(gvc_cache->gvc_list)
    {
      p_drop_gvahand_cache_elem1(gvc_cache);
    }
  }
  global_gvcache = NULL;

  if(gap_debug)
  {
    printf("p_drop_vidhandle_cache END\n");
  }
  

}  /* end p_drop_vidhandle_cache */
#endif




#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
/* ----------------------------------------------------
 * p_ffetch_get_open_gvahand
 * ----------------------------------------------------
 * get videohandle from the cache of already open handles.
 * opens a new handle if there is no cache hit.
 * if too many handles are alredy open, the oldest one is closed.
 * note that the cached handles must not be used outside the
 * frame fetcher module. Therefore the closing old handles
 * can be done.
 */
static t_GVA_Handle*
p_ffetch_get_open_gvahand(const char* filename, gint32 seltrack, const char *preferred_decoder)
{
  gint32 l_idx;
  t_GVA_Handle *l_gvahand;
  GapFFetchGvahandCacheElem  *gvc_ptr;
  GapFFetchGvahandCacheElem  *gvc_last;
  GapFFetchGvahandCacheElem  *gvc_new;
  GapFFetchGvahandCache      *gvcache;

  if(filename == NULL)
  {
    printf("p_ffetch_get_open_gvahand: ** ERROR cant load video filename == NULL!\n");
    return (NULL);
  }

  if(global_gvcache == NULL)
  {
    /* init the global_image cache */
    global_gvcache = g_malloc0(sizeof(GapFFetchGvahandCache));
    global_gvcache->gvc_list = NULL;
    global_gvcache->max_vid_cache = p_get_ffetch_max_gvc_cache_elements();
  }

  gvcache = global_gvcache;
  gvc_last = gvcache->gvc_list;

  l_idx = 0;
  for(gvc_ptr = gvcache->gvc_list; gvc_ptr != NULL; gvc_ptr = (GapFFetchGvahandCacheElem *)gvc_ptr->next)
  {
    l_idx++;
    if((strcmp(filename, gvc_ptr->filename) == 0) && (seltrack == gvc_ptr->seltrack))
    {
      /* videohandle found in cache, can skip opening a new handle */
      return(gvc_ptr->gvahand);
    }
    gvc_last = gvc_ptr;
  }

  if(preferred_decoder)
  {
    l_gvahand = GVA_open_read_pref(filename
                                  , seltrack
                                  , 1 /* aud_track */
                                  , preferred_decoder
                                  , FALSE  /* use MMX if available (disable_mmx == FALSE) */
                                  );
  }
  else
  {
    l_gvahand = GVA_open_read(filename
                             ,seltrack
                             ,1 /* aud_track */
                             );
  }
  
  if(l_gvahand)
  {
    GVA_set_fcache_size(l_gvahand, p_get_ffetch_gva_frames_to_keep_cached());

    gvc_new = g_malloc0(sizeof(GapFFetchGvahandCacheElem));
    gvc_new->filename = g_strdup(filename);
    gvc_new->seltrack = seltrack;
    gvc_new->mtime = gap_file_get_mtime(filename);
    gvc_new->gvahand = l_gvahand;

    if(gvcache->gvc_list == NULL)
    {
      gvcache->gvc_list = gvc_new;   /* 1.st elem starts the list */
    }
    else
    {
      gvc_last->next = (GapFFetchGvahandCacheElem *)gvc_new;  /* add new elem at end of the cache list */
    }

    if(l_idx > gvcache->max_vid_cache)
    {
      /* cache list has more elements than desired,
       * drop the 1.st (oldest) entry in the cache list
       * (this closes the droped handle)
       */
      p_drop_gvahand_cache_elem1(gvcache);
    }
  }
  return(l_gvahand);
}  /* end p_ffetch_get_open_gvahand */
#endif

/* -----------------------------------------
 * p_add_image_to_list_of_duplicated_images
 * -----------------------------------------
 * add specified image to the list of duplicated images.
 * this list contains temporary images of both fetched video frames
 * and merged duplicates of the cached original images.
 */
static void
p_add_image_to_list_of_duplicated_images(gint32 image_id, gint32 ffetch_user_id)
{
  GimpParasite  *l_parasite;

  /* attach a parasite to mark the image as part of the gap image duplicates cache */
  l_parasite = gimp_parasite_new(GAP_IMAGE_DUP_CACHE_PARASITE
                                 ,0  /* GIMP_PARASITE_PERSISTENT  0 for non persistent */
                                 , sizeof(gint32)     /* size of parasite data */
                                 ,&ffetch_user_id     /* parasite data */
                                 );

  if(l_parasite)
  {
    gimp_image_parasite_attach(image_id, l_parasite);
    gimp_parasite_free(l_parasite);
  }
}   /* end p_add_image_to_list_of_duplicated_images */


/* -------------------------------------------------
 * gap_frame_fetch_delete_list_of_duplicated_images
 * -------------------------------------------------
 * deletes all duplicate imageas that wre created by the specified ffetch_user_id
 * (if ffetch_user_id -1 is specified delte all duplicated images)
 */
void
gap_frame_fetch_delete_list_of_duplicated_images(gint32 ffetch_user_id)
{
  gint32 *images;
  gint    nimages;
  gint    l_idi;

  images = gimp_image_list(&nimages);
  for(l_idi=0; l_idi < nimages; l_idi++)
  {
    GimpParasite  *l_parasite;
  
    l_parasite = gimp_image_parasite_find(images[l_idi], GAP_IMAGE_DUP_CACHE_PARASITE);

    if(gap_debug)
    {
      printf("FrameFetcher: check (image_id:%d) name:%s pid:%d\n"
            , (int)images[l_idi]
            , gimp_image_get_filename(images[l_idi])
            , (int)gap_base_getpid()
            );
    }

    if(l_parasite)
    {
      gint32 *ffetch_user_id_ptr;
      
      ffetch_user_id_ptr = (gint32 *) l_parasite->data;
      if((*ffetch_user_id_ptr == ffetch_user_id) || (ffetch_user_id < 0))
      {
        if(gap_debug)
        {
          printf("FrameFetcher: DELETE duplicate %s (image_id:%d) user_id:%d (%d)  name:%s pid:%d\n"
                , gimp_image_get_filename(images[l_idi])
                , (int)images[l_idi]
                , (int)ffetch_user_id
                , (int)*ffetch_user_id_ptr
                , gimp_image_get_filename(images[l_idi])
                , (int)gap_base_getpid()
                );
        }
        /* delete image from the duplicates cache */
        gap_image_delete_immediate(images[l_idi]);
      }
      gimp_parasite_free(l_parasite);
    }
  }
  if(images)
  {
    g_free(images);
  }

}  /* end gap_frame_fetch_delete_list_of_duplicated_images */




/* ----------------------------
 * gap_frame_fetch_orig_image
 * ----------------------------
 * returns image_id of the original cached image.
 *    RESTRICTION: the Caller must NOT not modify that image and shall not open a display for it!
 */
gint32
gap_frame_fetch_orig_image(gint32 ffetch_user_id
    ,const char *filename            /* full filename of the image */
    ,gboolean addToCache             /* enable caching */
    )
{
  return (p_load_cache_image(filename, ffetch_user_id, addToCache));
}  /* end gap_frame_fetch_orig_image */


/* -------------------------------
 * gap_frame_fetch_image_scale
 * -------------------------------
 */
void
gap_frame_fetch_image_scale(gint32 imageId, gint32 prescaleWidth, gint32 prescaleHeight)
{
  if(gap_debug)
  {
    char          *l_filename;
    l_filename = gimp_image_get_filename(imageId);
    
    printf("gap_frame_fetch_image_SCALE id:%d (%dx%d) to newSize ==> (%dx%d) %s\n"
      ,(int)imageId
      ,(int)gimp_image_width(imageId)
      ,(int)gimp_image_height(imageId)
      ,(int)prescaleWidth
      ,(int)prescaleHeight
      ,l_filename
      );

    if(l_filename != NULL)
    {
      g_free(l_filename);
    }
  }
  gimp_image_undo_disable(imageId);
  gimp_image_scale(imageId, prescaleWidth, prescaleHeight);

}  /* end gap_frame_fetch_image_scale */

/* -------------------------------
 * gap_frame_fetch_image_duplicate
 * -------------------------------
 */
gint32
gap_frame_fetch_image_duplicate(gint32 imageId)
{
  gint32 dupImageId;
  if(gap_debug)
  {
    char          *l_filename;
    l_filename = gimp_image_get_filename(imageId);
    
    printf("gap_frame_fetch_image_DUPLICATE id:%d (%dx%d) %s\n"
      ,(int)imageId
      ,(int)gimp_image_width(imageId)
      ,(int)gimp_image_height(imageId)
      ,l_filename
      );

    if(l_filename != NULL)
    {
      g_free(l_filename);
    }
  }
  
  dupImageId = gimp_image_duplicate(imageId);
  return (dupImageId);
}


/* -------------------------------
 * p_duplicate_image_for_prescale
 * -------------------------------
 * duplicate the specified image
 * and mark the duplicate as "prescaleEnabled" 
 */
static gint32
p_duplicate_image_for_prescale(gint32 origsizeImageId)
{
  gint32 retImageId;
  GimpParasite  *l_parasite;

  retImageId = gap_frame_fetch_image_duplicate(origsizeImageId);

  /* change the type information in the image parasite in the copy */
  l_parasite = gimp_image_parasite_find(retImageId, GAP_IMAGE_CACHE_PARASITE);
  if (l_parasite)
  {
    GapImageCacheParasitePointers para;
    GapImageCacheParasitePointers *paraPtr;

    paraPtr = &para;
    p_init_GapImageCacheParasitePointers(paraPtr, l_parasite->data);

    gap_frame_fetch_remove_parasite(retImageId);

    *(paraPtr->type_ptr) = GAP_IMAGE_CACHE_TYPE_PRESCALE_ENABLED;
    gimp_image_parasite_attach(retImageId, l_parasite);

    gimp_parasite_free(l_parasite);
  }
  return (retImageId);
  
}  /* end p_duplicate_image_for_prescale */


/* -------------------------------
 * gap_frame_fetch_prescaled_image
 * -------------------------------
 * returns image_id of the prescaled cached image.
 *    RESTRICTION: the Caller must NOT not modify that image and shall not open a display for it!
 * Note that the returned image can be either the prescaled copy or the cached image at
 * original size (in case prescale Size not yet known or exactly matching original size.
 */
gint32
gap_frame_fetch_prescaled_image(gint32 ffetch_user_id
    ,const char *filename            /* full filename of the image */
    ,gboolean addToCache             /* enable caching on prescaled image */
    ,gint32 prescaleWidth            /* use 0 in case prescale size is not yet known */
    ,gint32 prescaleHeight           /* use 0 in case prescale size is not yet known */
    ,gint32 *originalWidthPtr        /* OUT: width of the unscaled original image file */
    ,gint32 *originalHeightPtr       /* OUT: width of the unscaled original image file */
    )
{
  GapImageChacheInfo  gapImageChacheInfo;
  gint32 retImageId = -1;
  gint32 prescaleImageId = -1;
  gint32 origsizeImageId = -1;

  if(gap_debug)
  {
    printf("gap_frame_fetch_prescaled_image START %s\n"
      ,filename
      );
  }
  /* search in cache for images marked as "prescaleEnabled" */
  prescaleImageId = p_find_cache_image(filename, ffetch_user_id
          , GAP_IMAGE_CACHE_TYPE_PRESCALE_ENABLED  /* cachedImageType */
          , &gapImageChacheInfo
          , originalWidthPtr
          , originalHeightPtr
          );

  if (prescaleImageId >= 0)
  {
     retImageId = prescaleImageId;
     gint32 pWidth = gimp_image_width(prescaleImageId);
     gint32 pHeight = gimp_image_height(prescaleImageId);

     /* check width for re-upscale attempt */
     if ((prescaleWidth > pWidth)
     &&  (*originalWidthPtr > pWidth))
     {
        retImageId = -1;
     }          
        
     /* check height for re-upscale attempt */
     if ((prescaleHeight > pHeight)
     &&  (*originalHeightPtr > pHeight))
     {
        retImageId = -1;
     }
     
     if (retImageId >= 0)
     {
       if ((pWidth != prescaleWidth)
       ||  (pHeight != prescaleHeight))
       {
         if ((prescaleWidth > 0)
         &&  (prescaleHeight >0))
         {
           gap_frame_fetch_image_scale(retImageId, prescaleWidth, prescaleHeight);
         }
       }
       return(retImageId);
     }

  
     if(gap_debug)
     {
       printf("gap_frame_fetch_prescaled_image DELETE img: %d from cache %s\n"
         ,(int)prescaleImageId
         ,filename
       );
     }
     /* remove prescaleImageId from cache.
      * The current requested prescale size is greater than the available
      * prescaled image in the cache AND the original is larger than the cached variante.
      * in this case remove the prescaled image from the cache
      * (to force re-caching of a larger version and avoid quality loss
      *  that could be caused by upscaling of an already downscaled image)
      */
     gap_image_delete_immediate(prescaleImageId);
  }

  /* search in cache for non prescaled image at original size (not marked as "prescaleEnabled" */
  origsizeImageId = p_find_cache_image(filename, ffetch_user_id
          , GAP_IMAGE_CACHE_TYPE_ORIGINAL_SIZE  /* cachedImageType */
          , &gapImageChacheInfo
          , originalWidthPtr
          , originalHeightPtr
          );
  if (origsizeImageId >= 0)
  {
    if ((gimp_image_width(origsizeImageId) == prescaleWidth)
    &&  (gimp_image_height(origsizeImageId) == prescaleHeight))
    {
      return (origsizeImageId);
    }
    retImageId = p_duplicate_image_for_prescale(origsizeImageId);
  }

  if (retImageId < 0)
  {
    origsizeImageId = p_load_image_and_add_to_cache(filename, ffetch_user_id
         , addToCache
         , GAP_IMAGE_CACHE_TYPE_ORIGINAL_SIZE  /* cachedImageType */
         , &gapImageChacheInfo
         , originalWidthPtr
         , originalHeightPtr
         );
    if (origsizeImageId >= 0)
    {
      if ((prescaleWidth <= 0)
      ||  (prescaleHeight <= 0))
      {
        return (origsizeImageId);
      }
      if ((gimp_image_width(origsizeImageId) == prescaleWidth)
      &&  (gimp_image_height(origsizeImageId) == prescaleHeight))
      {
        return (origsizeImageId);
      }
      
    
      retImageId = p_duplicate_image_for_prescale(origsizeImageId);
    }
  }

  if (retImageId >= 0)
  {
     if ((gimp_image_width(retImageId) != prescaleWidth)
     ||  (gimp_image_height(retImageId) != prescaleHeight))
     {
       if ((prescaleWidth > 0)
       &&  (prescaleHeight >0))
       {
         gap_frame_fetch_image_scale(retImageId, prescaleWidth, prescaleHeight);
       }
     }
  }

  return (retImageId);
  

}  /* end gap_frame_fetch_prescaled_image */



/* ----------------------------
 * gap_frame_fetch_dup_image
 * ----------------------------
 * returns merged or selected layer_id 
 *        (that is the only visible layer in temporary created scratch image)
 *        the caller is resonsible to delete the scratch image when processing is done.
 *         this can be done by calling gap_frame_fetch_delete_list_of_duplicated_images()
 */
gint32
gap_frame_fetch_dup_image(gint32 ffetch_user_id
    ,const char *filename            /* full filename of the image (already contains framenr) */
    ,gint32      stackpos            /* 0 pick layer on top of stack, -1 merge visible layers */
    ,gboolean addToCache             /* enable caching */
    )
{
  gint32 resulting_layer;
  gint32 image_id;
  gint32 dup_image_id;

  resulting_layer = -1;
  dup_image_id = -1;
  image_id = p_load_cache_image(filename, ffetch_user_id, addToCache);
  if (image_id < 0)
  {
    return(-1);
  }
  
  if (stackpos < 0)
  {
    dup_image_id = gap_frame_fetch_image_duplicate(image_id);

    gap_frame_fetch_remove_parasite(dup_image_id);
    resulting_layer = gap_image_merge_visible_layers(dup_image_id, GIMP_CLIP_TO_IMAGE);
  }
  else
  {
    gint          l_nlayers;
    gint32       *l_layers_list;
     

    l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
    if(l_layers_list != NULL)
    {
      if (stackpos < l_nlayers)
      {
        gint32 src_layer_id;
        gdouble    l_xresoulution, l_yresoulution;
        gint32     l_unit;

        src_layer_id = l_layers_list[stackpos];
        dup_image_id = gimp_image_new (gimp_image_width(image_id)
                                     , gimp_image_height(image_id)
                                     , gimp_image_base_type(image_id)
                                     );
        gimp_image_get_resolution(image_id, &l_xresoulution, &l_yresoulution);
        gimp_image_set_resolution(dup_image_id, l_xresoulution, l_yresoulution);

        l_unit = gimp_image_get_unit(image_id);
        gimp_image_set_unit(dup_image_id, l_unit);
        
        resulting_layer = gap_layer_copy_to_image (dup_image_id, src_layer_id);
      }
      
      g_free (l_layers_list);
    }
  }

  p_add_image_to_list_of_duplicated_images(dup_image_id, ffetch_user_id);


  if (addToCache != TRUE)
  {
    GimpParasite  *l_parasite;

    l_parasite = gimp_image_parasite_find(image_id, GAP_IMAGE_CACHE_PARASITE);

    if(l_parasite)
    {
      gimp_parasite_free(l_parasite);
    }
    else
    {
      /* the original image is not cached
       * (delete it because the caller gets the preprocessed duplicate)
       */
      gap_image_delete_immediate(image_id);
    }
    
  }

  return(resulting_layer);

}  /* end gap_frame_fetch_dup_image */



/* ----------------------------
 * gap_frame_fetch_dup_video
 * ----------------------------
 * returns the fetched video frame as gimp layer_id.
 *         the returned layer id is the only layer in a temporary image.
 *         note that the caller is responsible to delete the temporary image after processing is done.
 *         this can be done by calling gap_frame_fetch_delete_list_of_duplicated_images()
 */
gint32
gap_frame_fetch_dup_video(gint32 ffetch_user_id
    ,const char *filename            /* full filename of a video */
    ,gint32      framenr             /* frame within the video (starting at 1) */
    ,gint32      seltrack            /* videotrack */
    ,const char *preferred_decoder)
{
  gint32         l_layer_id = -1;
#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
  t_GVA_Handle *gvahand;
  t_GVA_RetCode  l_fcr;
  
  gvahand = p_ffetch_get_open_gvahand(filename, seltrack, preferred_decoder);
  if (gvahand == NULL)
  {
    return(-1);
  }

  /* attempt to get frame from the handles internal cache */
  l_fcr = GVA_frame_to_gimp_layer(gvahand
                                    , TRUE      /* delete_mode */
                                    , framenr   /* framenumber */
                                    , 0         /* deinterlace */
                                    , 0.0       /* threshold */
                                    );

  if (l_fcr != GVA_RET_OK)
  {
    /* if no success, we try explicite read that frame  */
    if(gvahand->current_seek_nr != framenr)
    {
      if(((gvahand->current_seek_nr + p_get_ffetch_gva_frames_to_keep_cached()) > framenr)
      &&  (gvahand->current_seek_nr < framenr ) )
      {
        /* near forward seek is performed by dummyreads to fill up the 
         * handles internal framecache
         */
        while(gvahand->current_seek_nr < framenr)
        {
          GVA_get_next_frame(gvahand);
        }
      }
      else
      {
        GVA_seek_frame(gvahand, (gdouble)framenr, GVA_UPOS_FRAMES);
     }
    }

    if(GVA_get_next_frame(gvahand) == GVA_RET_OK)
    {
      GVA_frame_to_gimp_layer(gvahand
                      , TRUE      /* delete_mode */
                      , framenr   /* framenumber */
                      , 0         /* deinterlace */
                      , 0.0       /* threshold */
                      );
    }
  }

  /* return the newly created layer from the temporary image in the gvahand stucture.
   */
  l_layer_id = gvahand->layer_id;
  
  p_add_image_to_list_of_duplicated_images(gvahand->image_id, ffetch_user_id);
  gvahand->image_id = -1;
  gvahand->layer_id = -1;

#endif  
  return (l_layer_id);

}  /* end gap_frame_fetch_dup_video */    


/* -------------------------------------------------
 * gap_frame_fetch_drop_resources
 * -------------------------------------------------
 * drop all cached resources and all working copies (in the list of duplicated images).
 * (regardless if there are still resource users registrated)
 */
void
gap_frame_fetch_drop_resources()
{
  gap_frame_fetch_delete_list_of_duplicated_images(-1);

  p_drop_image_cache();
#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
  p_drop_vidhandle_cache();
#endif
}  /* end gap_frame_fetch_drop_resources */


/* -------------------------------------------------
 * gap_frame_fetch_register_user
 * -------------------------------------------------
 * register for using the frame fetcher resource.
 * returns a unique resource user id.
 */
gint32
gap_frame_fetch_register_user(const char *caller_name)
{
  gint32 max_ffetch_user_id;
  GapFFetchResourceUserElem *usr_ptr;
  GapFFetchResourceUserElem *new_usr_ptr;
  
  max_ffetch_user_id = 0;
  new_usr_ptr = NULL;
  
  for(usr_ptr = global_rsource_users; usr_ptr != NULL; usr_ptr = (GapFFetchResourceUserElem *)usr_ptr->next)
  {
    /* printf("usr_ptr->ffetch_user_id: %d  usr_ptr:%d\n", usr_ptr->ffetch_user_id, usr_ptr); */
  
    if (usr_ptr->ffetch_user_id >= 0)
    {
      max_ffetch_user_id = MAX(max_ffetch_user_id, usr_ptr->ffetch_user_id);
    }
    else
    {
      new_usr_ptr = usr_ptr;  /* reuse inactive element */
    }
  }
  
  max_ffetch_user_id++;
  if (new_usr_ptr == NULL)
  {
    new_usr_ptr = g_new(GapFFetchResourceUserElem, 1);
    new_usr_ptr->next = global_rsource_users;
    global_rsource_users = new_usr_ptr;
  }
  new_usr_ptr->ffetch_user_id = max_ffetch_user_id;
  
  if(gap_debug)
  {
    printf("gap_frame_fetch_register_user: REGISTRATED ffetch_user_id:%d  caller_name:%s  new_usr_ptr:%d pid:%d\n"
          , new_usr_ptr->ffetch_user_id
          , caller_name
          , (int)&new_usr_ptr
          , (int) gap_base_getpid()
          );
  }
  return (max_ffetch_user_id);
}  /* end  gap_frame_fetch_register_user*/


/* -------------------------------------------------
 * gap_frame_fetch_unregister_user
 * -------------------------------------------------
 * unregister the specified resource user id.
 * (if there are still registered resource users
 *  cached images and videohandles are kept.
 *  until the last resource user calls this procedure.
 *  if there are no more registered users all
 *  cached videohandle resources and temporary image duplicates are dropped)
 * Current restriction:
 *  the current implementation keeps user registration in global data
 *  but filtermacros and storyboard processor typically are running in separate
 *  processes an therefore each process has its own global data.
 *  an empty list of users does not really indicate
 *  that there are no more users (another process may still have users
 *  of cached images)
 *  therefore cached images are NOT dropped
 */
void
gap_frame_fetch_unregister_user(gint32 ffetch_user_id)
{
  gint32 count_active_users;
  GapFFetchResourceUserElem *usr_ptr;

  if(gap_debug)
  {
    printf("gap_frame_fetch_unregister_user: UNREGISTER ffetch_user_id:%d pid:%d\n"
          , ffetch_user_id
          , (int) gap_base_getpid()
          );
  }

  count_active_users = 0;
  for(usr_ptr = global_rsource_users; usr_ptr != NULL; usr_ptr = (GapFFetchResourceUserElem *)usr_ptr->next)
  {
    if (ffetch_user_id == usr_ptr->ffetch_user_id)
    {
      usr_ptr->ffetch_user_id = -1;
    }
    else if (usr_ptr->ffetch_user_id >= 0)
    {
      count_active_users++;
    }
  }
  
  if(count_active_users == 0)
  {
    if(gap_debug)
    {
      printf("gap_frame_fetch_unregister_user: no more resource users, DROP cached duplicates and video handles\n");
    }
    gap_frame_fetch_delete_list_of_duplicated_images(-1);
#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
    p_drop_vidhandle_cache();
#endif
  }

}  /* end gap_frame_fetch_unregister_user */


/* ---------------------------------
 * gap_frame_fetch_is_image_in_cache
 * ---------------------------------
 * checks the image for presence of the parasite that marks the image as member
 * of the gap frame fetcher cache.
 * return TRUE if the parasite was found (e.g. image is cache member)
 */
gboolean
gap_frame_fetch_is_image_in_cache(gint32 image_id)
{
  GimpParasite  *l_parasite;
 
  l_parasite = gimp_image_parasite_find(image_id, GAP_IMAGE_CACHE_PARASITE);

  if(l_parasite)
  {
    gimp_parasite_free(l_parasite);
    return(TRUE);  /* isCacheMember */
  }
  
  return (FALSE);
  
}  /* end gap_frame_fetch_is_image_in_cache */


/* -------------------------------
 * gap_frame_fetch_remove_parasite
 * -------------------------------
 * removes the image parasite that marks the image as member
 * of the gap frame fetcher cache.
 */
void
gap_frame_fetch_remove_parasite(gint32 image_id)
{
  GimpParasite  *l_parasite;
 
  l_parasite = gimp_image_parasite_find(image_id, GAP_IMAGE_CACHE_PARASITE);

  if(l_parasite)
  {
    gimp_image_parasite_detach(image_id, GAP_IMAGE_CACHE_PARASITE);
    if(gap_debug)
    {
      printf("FrameFetcher: removed parasite from (image_id:%d) pid:%d\n"
        , (int)image_id, (int)gap_base_getpid());
    }
    gimp_parasite_free(l_parasite);
  }

}  /* end gap_frame_fetch_remove_parasite */




/* ----------------------------------------------------
 * p_dump_resources_gvahand
 * ----------------------------------------------------
 * print videohandle resource usage of cached video handles to stdout.
 */
static void
p_dump_resources_gvahand()
{
#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
  GapFFetchGvahandCacheElem  *gvc_ptr;
  GapFFetchGvahandCacheElem  *gvc_last;
  GapFFetchGvahandCache      *gvcache;
  gint32                      count;

  count = 0;
  if(global_gvcache != NULL)
  {
    gvcache = global_gvcache;
    gvc_last = gvcache->gvc_list;

    for(gvc_ptr = gvcache->gvc_list; gvc_ptr != NULL; gvc_ptr = (GapFFetchGvahandCacheElem *)gvc_ptr->next)
    {
      t_GVA_Handle *gvahand;
      
      gvahand = gvc_ptr->gvahand;
      
      count++;
      printf("FrameFetcher GVA_handle: %s   currFrameNr:%d fcache elemSize:%d byteSize:%d\n"
        , gvahand->filename
        , (int) gvahand->current_frame_nr
        , (int)GVA_get_fcache_size_in_elements(gvahand)
        , (int)GVA_get_fcache_size_in_bytes(gvahand)
        );

      gvc_last = gvc_ptr;
    }
    printf("FrameFetcher holds %d open GVA_handles limit gap_ffetch_max_gvc_cache_elements:%d (fcache_size:%d)\n"
       ,(int)count
       ,(int)p_get_ffetch_max_gvc_cache_elements()
       ,(int)p_get_ffetch_gva_frames_to_keep_cached
       );
    
  }
  else
  {
    printf("FrameFetcher has no GVA_handle cached\n");
  }
#else

  printf("FrameFetcher compiled without GAP_ENABLE_VIDEOAPI_SUPPORT\n");

#endif

}  /* end p_dump_resources_gvahand */




/* ----------------------------------------------------
 * p_dump_process_resource_usage
 * ----------------------------------------------------
 * print memory resources used (in case sysinfo was available on compiletime)
 *
 * NOTE: 
 *   getrusage did not work in 1st test on linux (and is not available on windows)
 *   while  sysinfo worked on my linux development environment, but was not availabe
 *   on Windows MinGW environment.
 *   therefore the configure script cheks for the sysinfo header and
 *   sets the GAP_HAVE_SYSINFO define constant to 1 when available.
 *      
 */
static void
p_dump_process_resource_usage()
{
#ifdef GAP_HAVE_SYSINFO

  int rc;
  struct sysinfo info;
  
  rc = sysinfo(&info);
  if(rc == 0)
  {
    printf ("FrameFetcher sysinfo memory in bytes:\n"
            "  memory total size:         %lu\n"
            "  memory free:               %lu\n"
            "  shared memory total size:  %lu\n"
            "  memory used by buffers:    %lu\n"
            "  swap total size:           %lu\n"
            "  swap free:                 %lu\n"
            "  high memory total size:    %lu\n"
            "  high memory free:          %lu\n"
      , (info.mem_unit * info.totalram)
      , (info.mem_unit * info.freeram)
      , (info.mem_unit * info.sharedram)
      , (info.mem_unit * info.bufferram)
      , (info.mem_unit * info.totalswap)
      , (info.mem_unit * info.freeswap)
      , (info.mem_unit * info.totalhigh)
      , (info.mem_unit * info.freehigh)
      );
  
  }
  else
  {
    printf("FrameFetcher sysinfo failed with retcode:%d\n", rc);
  }


#else
  printf("** WARNING FrameFetcher sysinfo memory resource usage NOT available "
         " because the FrameFetcher was compiled without sysinfo support (GAP_HAVE_SYSINFO not dfined)\n");
#endif
}

/* ----------------------------------------------------
 * gap_frame_fetch_dump_resources
 * ----------------------------------------------------
 * print current resource usage to stdout
 * this includes information about 
 *  - ALL images currently loaded
 *  - all video filehandles with memory cache sizes
 * 
 */
void
gap_frame_fetch_dump_resources()
{
  gint32 *images;
  gint    nimages;
  gint    l_idi;
  gint    l_number_of_cached_images;


  printf("gap_frame_fetch_dump_resources: START pid:%d\n", (int)gap_base_getpid());


  l_number_of_cached_images = 0;
  images = gimp_image_list(&nimages);
  
  
  
  for(l_idi=0; l_idi < nimages; l_idi++)
  {
    GimpParasite  *l_parasite;
    char          *l_filename;
    char          *l_cacheInfoString;
    gint32         image_id;
    
    
    image_id = images[l_idi];
    l_filename = gimp_image_get_filename(image_id);
    l_parasite = gimp_image_parasite_find(image_id, GAP_IMAGE_CACHE_PARASITE);

    if(l_parasite)
    {
      GapImageCacheParasitePointers para;
      GapImageCacheParasitePointers *paraPtr;
      
      paraPtr = &para;
      p_init_GapImageCacheParasitePointers(paraPtr, l_parasite->data);

    
      l_number_of_cached_images++;

      l_cacheInfoString = g_strdup_printf("Cache member: mtime:%d ffetchId:%d %s"
                                         ,*(paraPtr->mtime_ptr)
                                         ,*(paraPtr->ffetch_id_ptr)
                                         ,paraPtr->filename_ptr
                                         );
      
      gimp_parasite_free(l_parasite);
    }
    else
    {
      l_parasite = gimp_image_parasite_find(images[l_idi], GAP_IMAGE_DUP_CACHE_PARASITE);
      if(l_parasite)
      {
        gint32 *ffetch_user_id_ptr;
        ffetch_user_id_ptr = (gint32 *) l_parasite->data;

        l_number_of_cached_images++;
  
        l_cacheInfoString = g_strdup_printf("Cache member (merged duplicate): ffetchId:%d"
                                         ,*ffetch_user_id_ptr
                                         );
        
        gimp_parasite_free(l_parasite);
      }
      else
      {
        l_cacheInfoString = g_strdup_printf("Not cached");
      }
    }

    
    printf(" FrameFetcher ImgId:%d (%d x %d) %s %s\n"
          ,(int)image_id
          ,(int)gimp_image_width(image_id)
          ,(int)gimp_image_height(image_id)
          ,l_filename
          ,l_cacheInfoString
          );
    
    g_free(l_cacheInfoString);
    if(l_filename != NULL)
    {
      g_free(l_filename);
    }
    l_parasite = NULL;
  }
  
  if(images)
  {
    g_free(images);
  }
  
  printf(" Number of images currently loaded in gimp total: %d gap_ffetch_max_img_cache_elements:%d marked as cache member:%d\n"
        ,(int)nimages
        ,(int)p_get_ffetch_max_img_cache_elements()
        ,(int)l_number_of_cached_images
        );


  p_dump_resources_gvahand();
  p_dump_process_resource_usage();

}  /* end gap_frame_fetch_dump_resources */

