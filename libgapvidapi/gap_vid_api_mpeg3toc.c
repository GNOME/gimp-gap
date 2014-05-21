/* this file is a modified variant
 * of the meg3toc.c Code that came with libmpeg3-1.8
 *
 * it is used to build the libmpeg3
 * compatible "Table of content" files implicitly
 * when using the GIMP-GAP specific GVA video API.
 * .TOC files are the libmpeg3 specific pendant to gvaindexes
 * and enable fast and exact positioning in the MPEG videofile
 * (but need one initial full scan to create)
 *
 * This variant of the toc file generation is used in GIMP-GAP
 * GUI Modules and provides Progress Feedback for the calling GUI
 * and has the option to STOP (cancel) TOC creation on GUI request.
 *
 * 
 *
 * 2004.03.06     (hof)  created
 */

#include "libmpeg3.h"
#include "mpeg3protos.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <glib/gstdio.h>

extern int gap_debug;


/* -----------------------------
 * p_get_total_frames_from_toc
 * -----------------------------
 */
static gint32
p_get_total_frames_from_toc(char *toc_filename, int vid_track)
{
  mpeg3_t *toc_handle;
  int      l_error_return;
  gint32   total_frames;

  total_frames  = 0;
  toc_handle = mpeg3_open(toc_filename, &l_error_return);


  if((toc_handle) && (l_error_return == 0))
  {
    if(mpeg3_has_video(toc_handle))
    {
      total_frames = mpeg3_video_frames(toc_handle, vid_track);
    }
    mpeg3_close(toc_handle);
  }    /* end for repeat_count */

  if(gap_debug)
  {
      printf("GVA: p_get_total_frames_from_toc libmpeg3 OPEN %s handle->toc_handle:%ld l_error_return:%d total_frames:%d\n"
            , toc_filename
            , (long)toc_handle
            , (int)l_error_return
            , (int)total_frames
            );
  }

  return (total_frames);

}  /* end p_get_total_frames_from_toc */



/*
 * Generate table of frame and sample offsets for editing.
 */

int
GVA_create_libmpeg3_toc(int argc, char *argv[]
                , t_GVA_Handle *gvahand
    , int *ret_total_frames)
{
        int i;
        char *src = 0, *dst = 0;
        int verbose = 0;
        gdouble progress_stepsize;

        progress_stepsize = 1.0 / (gdouble)(MAX(gvahand->total_frames, 1000));
        gvahand->percentage_done = 0.05;
        if(gap_debug)
        {
          printf("GVA_create_libmpeg3_toc: gvahand->total_frames: %d  stepsize:%.6f\n"
               , (int)gvahand->total_frames
               , (float)progress_stepsize
               );
        }
        *ret_total_frames = -1; // hof: 



        if(argc < 3)
        {
                fprintf(stderr, "Table of contents generator version %d.%d.%d\n"
                        "Create a table of contents for a DVD or mpeg stream.\n"
                        "Usage: mpeg3toc <path> <output>\n"
                        "\n"
                        "-v Print tracking information\n"
                        "\n"
                        "The path should be absolute unless you plan\n"
                        "to always run your movie editor from the same directory\n"
                        "as the filename.  For renderfarms the filesystem prefix\n"
                        "should be / and the movie directory mounted under the same\n"
                        "directory on each node.\n\n"
                        "Example: mpeg3toc -v /cdrom/video_ts/vts_01_0.ifo titanic.toc\n",
                        mpeg3_major(),
                        mpeg3_minor(),
                        mpeg3_release());
                exit(1);
        }

        for(i = 1; i < argc; i++)
        {
                if(!strcmp(argv[i], "-v"))
                {
                        verbose = 1;
                }
                else
                if(argv[i][0] == '-')
                {
                        fprintf(stderr, "Unrecognized command %s\n", argv[i]);
                        exit(1);
                }
                else
                if(!src)
                {
                        src = argv[i];
                }
                else
                if(!dst)
                {
                        dst = argv[i];
                }
                else
                {
                        fprintf(stderr, "Ignoring argument \"%s\"\n", argv[i]);
                }
        }

        if(!src)
        {
                fprintf(stderr, "source path not supplied.\n");
                exit(1);
        }

        if(!dst)
        {
                fprintf(stderr, "destination path not supplied.\n");
                exit(1);
        }



        int64_t total_bytes;
        mpeg3_t *file = mpeg3_start_toc(src, dst, &total_bytes);
        if(!file)
        {
          return 1;;
        }
        
        while(1)
        {
                int64_t bytes_processed = 0;
                mpeg3_do_toc(file, &bytes_processed);

                /* hof: handle GUI user cancel request */
                {
                      gvahand->percentage_done = CLAMP((bytes_processed / MAX(1, total_bytes)), 0.0, 1.0);
                      gvahand->cancel_operation = (*gvahand->fptr_progress_callback)(gvahand->percentage_done, gvahand->progress_cb_user_data);
                      if(gap_debug)
                      {
                        printf("MPEG3TOC: CANCEL(v):%d FPTR:%ld\n"
                              , (int)gvahand->cancel_operation
                              , (long)gvahand->fptr_progress_callback
                              );
                      }
                      if(gvahand->cancel_operation)
                      {
                         mpeg3_stop_toc(file);
                         return 1;
                      }
                }
            

                if(bytes_processed >= total_bytes) break;
        }

        mpeg3_stop_toc(file);


        *ret_total_frames = p_get_total_frames_from_toc(dst, (int)gvahand->vid_track);

        return 0;
}
