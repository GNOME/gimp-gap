/* gap_frame_fetcher.h
 *
 *
 *  The FrameFetcher provides access to frames both from imagefiles and videofiles.
 *  
 *  It holds a global image cache of temporary gimp images intended for
 *  read only access in various gimp-gap render processings.
 *  
 *  There are methods to get the temporary image 
 *  or to get a duplicate that has only one layer at imagesize.
 *  (merged or picked via desired stackposition)
 *
 *  For videofiles it holds a cache of open videofile handles.
 *  (note that caching of videoframes is already available in the videohandle)
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _GAP_FRAME_FETCHER_H
#define _GAP_FRAME_FETCHER_H

#include "libgimp/gimp.h"

#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
#include "gap_vid_api.h"
#else
#ifndef GAP_STUBTYPE_GVA_HANDLE
typedef gpointer t_GVA_Handle;
#define GAP_STUBTYPE_GVA_HANDLE
#endif
#endif

/* -------------------------------------------------
 * gap_frame_fetch_register_user
 * -------------------------------------------------
 * register for using the frame fetcher resource.
 * returns a unique resource user id.
 */
gint32
gap_frame_fetch_register_user(const char *caller_name);

/* -------------------------------------------------
 * gap_frame_fetch_unregister_user
 * -------------------------------------------------
 * unregister the specified resource user id.
 + (if there are still registered resource users
 *  cached images and videohandles are kept.
 *  until the last resource user calls this procedure.
 *  if there are no more registered users all
 *  cached resources and duplicates are dropped)
 */
void
gap_frame_fetch_unregister_user(gint32 user_id);

/* -------------------------------------------------
 * gap_frame_fetch_drop_resources
 * -------------------------------------------------
 * drop all cached resources
 * (regardless if there are still resource users registrated)
 */
void
gap_frame_fetch_drop_resources();

/* -------------------------------------------------
 * gap_frame_fetch_delete_list_of_duplicated_images
 * -------------------------------------------------
 * deletes all duplicate imageas that wre created by the specified ffetch_user_id
 * (if ffetch_user_id -1 is specified delte all duplicated images)
 */
void
gap_frame_fetch_delete_list_of_duplicated_images(gint32 ffetch_user_id);


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
    );

/* -------------------------------
 * gap_frame_fetch_prescaled_image
 * -------------------------------
 * returns image_id of the prescaled cached image.
 * NOTE: the returned image MUST NOT not be changed by the caller
 *
 */
gint32
gap_frame_fetch_prescaled_image(gint32 ffetch_user_id
    ,const char *filename            /* full filename of the image */
    ,gboolean addToCache             /* enable caching on prescaled image */
    ,gint32 prescaleWidth            /* use 0 in case prescale size is not yet known */
    ,gint32 prescaleHeight           /* use 0 in case prescale size is not yet known */
    ,gint32 *originalWidthPtr        /* OUT: width of the unscaled original image file */
    ,gint32 *originalHeightPtr       /* OUT: width of the unscaled original image file */
    );

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
    );


/* ----------------------------
 * gap_frame_fetch_dup_video
 * ----------------------------
 * returns the fetched video frame as gimp layer_id.
 *         the returned layer id is (the only layer) in a temporary image.
 *         note the caller is responsible to delete that temporary image after processing is done.
 *         this can be done by calling gap_frame_fetch_delete_list_of_duplicated_images()
 */
gint32
gap_frame_fetch_dup_video(gint32 ffetch_user_id
    ,const char *filename            /* full filename of a video */
    ,gint32      framenr             /* frame within the video (starting at 1) */
    ,gint32      seltrack            /* videotrack */
    ,const char *preferred_decoder
    );

/* -------------------------------
 * gap_frame_fetch_image_scale
 * -------------------------------
 */
void
gap_frame_fetch_image_scale(gint32 imageId, gint32 width, gint32 height);

/* -------------------------------
 * gap_frame_fetch_image_duplicate
 * -------------------------------
 */
gint32
gap_frame_fetch_image_duplicate(gint32 imageId);


/* ---------------------------------
 * gap_frame_fetch_is_image_in_cache
 * ---------------------------------
 * checks the image for presence of the parasite that marks the image as member
 * of the gap frame fetcher cache.
 * return TRUE if the parasite was found (e.g. image is cache member)
 */
gboolean
gap_frame_fetch_is_image_in_cache(gint32 image_id);

/* -------------------------------
 * gap_frame_fetch_remove_parasite
 * -------------------------------
 * removes the image parasite that marks the image as member
 * of the gap frame fetcher cache.
 */
void
gap_frame_fetch_remove_parasite(gint32 image_id);


/* ----------------------------------------------------
 * gap_frame_fetch_dump_resources
 * ----------------------------------------------------
 * print current resource usage to stdout
 * this includes information about 
 *  - ALL images currently loaded in gimp
 *  - all video filehandles with memory cache sizes
 * 
 */
void
gap_frame_fetch_dump_resources();


#endif
