/*  gimplastvaldesc.c
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains:
 *   Procedures to register a plugin's LAST_VALUES buffer description
 *   (needed for animated filtercalls using a common iterator procedure)
 *
 *   should be a part of future libgimp for easy use in many plugin's.
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
 *          2003/09/20   hof: added datatype support for guint guint32.
 *          2003/01/19   hof: use gimp_directory (replace gimprc query for "gimp_dir")
 *          2002/04/07   hof: created.
 */

/* SYSTEM (UNIX) includes */ 
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib/gstdio.h>

#include "libgimp/gimp.h"
/* gimplastvaldesc should become part of libgimp in the future.
 * if this will come true, HAVE_LASTVALDESC_H  will also become DEFINED.
 */
#ifdef HAVE_LASTVALDESC_H
#include "libgimp/gimplastvaldesc.h"
#else
  /* for now GAP Sources include a private version of gimplastvaldesc.h gimplastvaldesc.c Modules
   * and must include them explicite because HAVE_LASTVALDESC_H is not defined
   * (i dont know, if gimplastvaldesc will be a part of libgimp someday  -- hof)
   */
#include "gimplastvaldesc.h"   
#endif
#include "gap_libgapbase.h"

typedef struct GimpLastvalTokenTabType
{
  char         *token;
  GimpLastvalType lastval_type;
} GimpLastvalTokenTabType;




static void      p_init_token_table(void);
static gchar *   p_load_lastval_desc_file(const gchar *fname);
static void      p_fwrite_lastvals_desc(FILE *fp, const gchar *keyname, GimpLastvalDescType *lastval_desc_arr, gint32 argc);
static gint32    p_fwrite_lines_until_keyname(FILE *fp, const char *keyname, gchar *ptr);
static void      p_fwrite_lines_remaining_without_keyname(FILE *fp, const char *keyname, gchar *ptr);
static void      p_lastvals_register_persistent(const gchar *keyname, GimpLastvalDescType *lastval_desc_arr, gint32 argc);


static GimpLastvalTokenTabType token_tab[GIMP_LASTVAL_END + 1];


gboolean
gimp_lastval_desc_register(const gchar *keyname, void  *baseadress, gint32 total_size, gint32 nlastvals, GimpLastvalDef *lastvals)
{
   void         *adress;
   gint32        elem_size;
   gint32        array_size;
   gchar        *elem_name;
   gint32        offset;
   GimpLastvalType  lastval_type;
   GimpLastvalDescType *lastval_desc_arr;
   gint          arg_cnt;
   gint          idx;
   gint          struct_cnt;
   gboolean      retcode;

   retcode = TRUE;
   p_init_token_table();

   arg_cnt = 0;
   struct_cnt = 0;
   offset = 0;
   array_size = 1;
   elem_size = 1;
   for(arg_cnt=0; arg_cnt < nlastvals; arg_cnt++)
   {
     lastval_type = lastvals[arg_cnt].type;

     switch(lastval_type)
     {
       case GIMP_LASTVAL_ARRAY:
         array_size = lastvals[arg_cnt].elem_size;
         if (array_size <= 0 || (offset + array_size) > total_size)
         {
            printf("ERROR gimp_lastval_desc_register: %s arg[%d] array_size: %d larger than total_size: %d\n"
                  ,keyname, (int)arg_cnt, (int)array_size, (int)total_size);
            retcode = FALSE;
         }
         break;
       case GIMP_LASTVAL_STRUCT_BEGIN:
         struct_cnt++;
         elem_size = lastvals[arg_cnt].elem_size;
         if (elem_size <= 0 || (offset + (array_size * elem_size)) > total_size)
         {
            printf("ERROR gimp_lastval_desc_register: %s arg[%d] offset %d + elem_size: %d * array_size: %d is larger than total_size: %d\n"
                  ,keyname, (int)arg_cnt, (int)offset, (int)elem_size, (int)array_size, (int)total_size);
            retcode = FALSE;
         }
         array_size = 1;
         break;
       case GIMP_LASTVAL_STRUCT_END:
         struct_cnt--;
         if (struct_cnt < 0)
         {
            printf("ERROR gimp_lastval_desc_register: %s arg[%d] STRUCT_BEGIN STRUCT_END out of balance\n \n"
                  ,keyname, (int)arg_cnt);
            break;  /* error: out of balance */
         }
         elem_size = lastvals[arg_cnt].elem_size;
         break;
       default:
         adress = lastvals[arg_cnt].elem_adress;
         offset = adress - baseadress;
         if (offset < 0 || offset >= total_size)
         {
            printf("ERROR gimp_lastval_desc_register: %s arg[%d] offset %d outside of structure adressrange\n"
                  ,keyname, (int)arg_cnt, (int)offset);
            retcode = FALSE;
         }
         elem_size = lastvals[arg_cnt].elem_size;
         if ((offset + (array_size * elem_size)) > total_size)
         {
            printf("ERROR gimp_lastval_desc_register: %s arg[%d] offset %d + elem_size: %d * array_size: %d is larger than total_size: %d\n"
                  ,keyname, (int)arg_cnt, (int)offset, (int)elem_size, (int)array_size, (int)total_size);
            retcode = FALSE;
         }
         array_size = 1;
         break;
     }
   }
   
   if(struct_cnt != 0)
   {
            printf("ERROR gimp_lastval_desc_register: %s arg[%d] STRUCT_BEGIN STRUCT_END out of balance\n"
                  ,keyname, (int)arg_cnt);
            retcode = FALSE;
   }

   if(retcode != TRUE)
   {
     return (retcode);
   }

   /* description + 2 automatic STRUCT BEGIN/END elemnts describing the total_size */
   lastval_desc_arr = g_new(GimpLastvalDescType, arg_cnt+2);
   
   /* 1.st entry always describes total_size of the LAST_VALUES Buffer structure */
   lastval_desc_arr[0].lastval_type = GIMP_LASTVAL_STRUCT_BEGIN;
   lastval_desc_arr[0].offset = 0;
   lastval_desc_arr[0].elem_size = total_size;
   lastval_desc_arr[0].iter_flag = GIMP_ITER_TRUE;
   lastval_desc_arr[0].elem_name[0] = '\0';

   for(arg_cnt=0, idx = 1; arg_cnt < nlastvals; arg_cnt++, idx++)
   {
     switch(lastvals[arg_cnt].type)
     {
       case GIMP_LASTVAL_ARRAY:
       case GIMP_LASTVAL_STRUCT_BEGIN:
       case GIMP_LASTVAL_STRUCT_END:
         lastval_desc_arr[idx].offset = 0;
         break;
       default:
         adress = lastvals[arg_cnt].elem_adress;
         lastval_desc_arr[idx].offset = adress - baseadress;
         break;
     }
     lastval_desc_arr[idx].elem_size    = lastvals[arg_cnt].elem_size;
     lastval_desc_arr[idx].iter_flag    = lastvals[arg_cnt].iter_flag;
     lastval_desc_arr[idx].lastval_type = lastvals[arg_cnt].type;     
     elem_name = lastvals[arg_cnt].elem_name;

     if(elem_name)
     {
       g_snprintf(lastval_desc_arr[idx].elem_name,
                  sizeof(lastval_desc_arr[idx].elem_name),
                   "%s", elem_name);
     }
     else
     {
       lastval_desc_arr[idx].elem_name[0]   = '\0';
     }
   }

   lastval_desc_arr[idx].lastval_type = GIMP_LASTVAL_STRUCT_END;
   lastval_desc_arr[idx].offset = 0;
   lastval_desc_arr[idx].elem_size = total_size;
   lastval_desc_arr[idx].iter_flag = GIMP_ITER_TRUE;
   lastval_desc_arr[idx].elem_name[0] = '\0';
   idx++;

   /* register for current session */
   /* 
    *{
    *   gchar *key_description;
    *
    *   key_description = gimp_lastval_desc_keyname(keyname);
    *   gimp_set_data(key_description, lastval_desc_arr, sizeof(GimpLastvalDescType) * idx); 
    *   g_free(key_description);
    *}
    */
   
   /* register permanent in a file */
   p_lastvals_register_persistent(keyname, lastval_desc_arr, idx);

   g_free(lastval_desc_arr);
   return (retcode);
}       /* gimp_lastval_desc_register */



#define TIMESTAMP_DESCFILE_CHECKED "GAP_KEYNAME_TIMESTAMP_DESCRIPTIONFILE_CHECKED"

void
gimp_lastval_desc_update(void)
{
  gchar *fname;
  gchar *file_buff;
  gchar *l_keyname;
  gchar *key_description;
  gint   arg_cnt;
  gint   max_expected_argc;
  GimpLastvalDescType *lastval_desc_arr;
  gchar   *ptr;
  gchar   *next_line_ptr;
  gint32 l_idx;
  gint32 l_filesize;
  long   l_timestamp;
  struct stat stat_buf;
  
  arg_cnt = 0;

  fname = gimp_lastval_desc_filename();  

  /* check timestamps */
  if(gimp_get_data_size(TIMESTAMP_DESCFILE_CHECKED) > 1)
  {     
     gimp_get_data(TIMESTAMP_DESCFILE_CHECKED, &l_timestamp);
     if(g_stat(fname, &stat_buf) == 0)
     {
        if(l_timestamp > stat_buf.st_mtime)
        {
           /* file last modification is older than timestamp of last check
            * (we dont need to read the file again)
            */
           g_free(fname);
           return;
        }
     }
     else
     {
        /* file not found, no need to continue */
        g_free(fname);
        return;
     }
  }
  l_timestamp = gap_base_get_current_time();
  gimp_set_data(TIMESTAMP_DESCFILE_CHECKED, &l_timestamp, sizeof(l_timestamp));
  
  p_init_token_table();
  
  /* read all descriptions from file  */
  file_buff = p_load_lastval_desc_file(fname);
  if(file_buff == NULL)
  {
    return;
  }
  l_filesize = strlen(file_buff);


  /* set/replace all descriptions in memory for the current gimp session */
  
  max_expected_argc = 10 + (l_filesize / 7);
  lastval_desc_arr = g_new(GimpLastvalDescType, max_expected_argc);
  l_keyname = NULL;
  key_description = g_strdup(" ");
  
  for(ptr = file_buff; *ptr != '\0'; ptr = next_line_ptr)
  {      
    /* skip blanks */
    while (*ptr == ' ' || *ptr == '\t') { ptr++;}

    /* findout start on next line (and terminate the current line with '\0') */
    next_line_ptr = ptr;
    while (*next_line_ptr != '\0')
    {
      if(*next_line_ptr == '\n')
      {
        *next_line_ptr = '\0';
        next_line_ptr++;
        break;
      }
      next_line_ptr++;
    }


    /* printf("LINE:%s\n", ptr); */

    /* ignore empty lines and comment lines */
    if ((*ptr != '\n')
    &&  (*ptr != '\0')
    &&  (*ptr != '#'))
    {
      if (*ptr == '"')
      {
        l_idx = 1;
        l_keyname = &ptr[1];
        while (1)
        {
          if (ptr[l_idx] == '"' || ptr[l_idx] == '\n' || ptr[l_idx] == '\0') 
          {
             ptr[l_idx] = '\0';
             g_free(key_description);
             /* printf("KEY:%s\n", l_keyname); */
             key_description = gimp_lastval_desc_keyname(l_keyname);
             break;
          }
          l_idx++;
        }
      }
      else
      {
        for(l_idx=0; l_idx <= GIMP_LASTVAL_END; l_idx++)
        {
          char *l_token;

          l_token = g_strdup_printf("%s;", token_tab[l_idx].token);
          if(0 == strncmp(ptr, l_token, strlen(l_token)))
          {
             /* found a matching datatype token */
             if(strcmp("END;",l_token) == 0)
             {
               lastval_desc_arr[arg_cnt].lastval_type = GIMP_LASTVAL_END;
               lastval_desc_arr[arg_cnt].offset = 0;
               lastval_desc_arr[arg_cnt].elem_size = 0;
               lastval_desc_arr[arg_cnt].iter_flag = 0;
               lastval_desc_arr[arg_cnt].elem_name[0] = '\0';
               arg_cnt++;
               /* if it is the END of description block store array in memory */
               /* printf("SET_DATA:%s\n", key_description); */
               gimp_set_data(key_description, lastval_desc_arr, sizeof(GimpLastvalDescType) * arg_cnt);
               arg_cnt = 0;
               g_free(key_description);
               key_description = g_strdup(" ");
             }
             else
             {
               int  l_nscan;

               lastval_desc_arr[arg_cnt].elem_name[0] = '\0';
               lastval_desc_arr[arg_cnt].lastval_type = token_tab[l_idx].lastval_type;
               l_nscan = sscanf(&ptr[strlen(l_token)], "%d;%d;%d;%50s"
                     , &lastval_desc_arr[arg_cnt].offset
                     , &lastval_desc_arr[arg_cnt].elem_size
                     , &lastval_desc_arr[arg_cnt].iter_flag
                     ,  lastval_desc_arr[arg_cnt].elem_name
                     );
               if(l_nscan != 3 && l_nscan != 4)
               {
                  printf("ERROR while scanning datatype %s in file %s\n", ptr, fname);
               }
               else
               {
                 arg_cnt++;
               }
             }
             break;
          }
          g_free(l_token);
        }
        if (l_idx > GIMP_LASTVAL_END)
        {
           printf("ERROR unknown datatype %s in file %s  %d  %d\n", ptr, fname, l_idx, GIMP_LASTVAL_END);
        }
      }
    }
  }

  g_free(key_description);
  g_free(lastval_desc_arr);
  g_free(fname);
}       /* end gimp_lastval_desc_update */





gchar *
gimp_lastval_desc_filename(void)
{
  gchar         *l_fname;

  l_fname = g_build_filename(gimp_directory (), "lastval_descriptions.txt", NULL);
  return(l_fname);
}       /* end gimp_lastval_desc_filename */



gchar *
gimp_lastval_desc_keyname(const gchar *keyname)
{
   return(g_strdup_printf("%s_ITER_DATA_DESCRIPTION", keyname));
}




/* -----------------------
 * local helper procedures
 * -----------------------
 */


static void
p_init_token_table(void)
{
  static gboolean  token_table_initialized = FALSE;

  if(token_table_initialized != TRUE)
  {
    /* typename strings */
    token_tab[GIMP_LASTVAL_NONE].token         = GIMP_DDESC_NONE;
    token_tab[GIMP_LASTVAL_ARRAY].token        = GIMP_DDESC_ARRAY;
    token_tab[GIMP_LASTVAL_STRUCT_BEGIN].token = GIMP_DDESC_STRUCT_BEGIN;
    token_tab[GIMP_LASTVAL_STRUCT_END].token   = GIMP_DDESC_STRUCT_END;

    token_tab[GIMP_LASTVAL_LONG].token             = GIMP_DDESC_LONG;
    token_tab[GIMP_LASTVAL_SHORT].token            = GIMP_DDESC_SHORT;
    token_tab[GIMP_LASTVAL_INT].token              = GIMP_DDESC_INT;
    token_tab[GIMP_LASTVAL_GINT].token             = GIMP_DDESC_GINT;
    token_tab[GIMP_LASTVAL_GINT32].token           = GIMP_DDESC_GINT32;
    token_tab[GIMP_LASTVAL_CHAR].token             = GIMP_DDESC_CHAR;
    token_tab[GIMP_LASTVAL_GCHAR].token            = GIMP_DDESC_GCHAR;
    token_tab[GIMP_LASTVAL_GUCHAR].token           = GIMP_DDESC_GUCHAR;
    token_tab[GIMP_LASTVAL_GDOUBLE].token          = GIMP_DDESC_GDOUBLE;
    token_tab[GIMP_LASTVAL_GFLOAT].token           = GIMP_DDESC_GFLOAT;
    token_tab[GIMP_LASTVAL_FLOAT].token            = GIMP_DDESC_FLOAT;
    token_tab[GIMP_LASTVAL_DOUBLE].token           = GIMP_DDESC_DOUBLE;
    token_tab[GIMP_LASTVAL_DRAWABLE].token         = GIMP_DDESC_DRAWABLE;
    token_tab[GIMP_LASTVAL_GINTDRAWABLE].token     = GIMP_DDESC_GINTDRAWABLE;
    token_tab[GIMP_LASTVAL_GBOOLEAN].token         = GIMP_DDESC_GBOOLEAN;
    token_tab[GIMP_LASTVAL_ENUM].token             = GIMP_DDESC_ENUM;
    token_tab[GIMP_LASTVAL_GUINT].token            = GIMP_DDESC_GUINT;
    token_tab[GIMP_LASTVAL_GUINT32].token          = GIMP_DDESC_GUINT32;
    token_tab[GIMP_LASTVAL_END].token              = GIMP_DDESC_END;

    /* type lastval_type informations */
    token_tab[GIMP_LASTVAL_NONE].lastval_type         = GIMP_LASTVAL_NONE;
    token_tab[GIMP_LASTVAL_ARRAY].lastval_type        = GIMP_LASTVAL_ARRAY;
    token_tab[GIMP_LASTVAL_STRUCT_BEGIN].lastval_type = GIMP_LASTVAL_STRUCT_BEGIN;
    token_tab[GIMP_LASTVAL_STRUCT_END].lastval_type   = GIMP_LASTVAL_STRUCT_END;

    token_tab[GIMP_LASTVAL_LONG].lastval_type             = GIMP_LASTVAL_LONG;
    token_tab[GIMP_LASTVAL_SHORT].lastval_type            = GIMP_LASTVAL_SHORT;
    token_tab[GIMP_LASTVAL_INT].lastval_type              = GIMP_LASTVAL_INT;
    token_tab[GIMP_LASTVAL_GINT].lastval_type             = GIMP_LASTVAL_GINT;
    token_tab[GIMP_LASTVAL_GINT32].lastval_type           = GIMP_LASTVAL_GINT32;
    token_tab[GIMP_LASTVAL_CHAR].lastval_type             = GIMP_LASTVAL_CHAR;
    token_tab[GIMP_LASTVAL_GCHAR].lastval_type            = GIMP_LASTVAL_GCHAR;
    token_tab[GIMP_LASTVAL_GUCHAR].lastval_type           = GIMP_LASTVAL_GUCHAR;
    token_tab[GIMP_LASTVAL_GDOUBLE].lastval_type          = GIMP_LASTVAL_GDOUBLE;
    token_tab[GIMP_LASTVAL_GFLOAT].lastval_type           = GIMP_LASTVAL_GFLOAT;
    token_tab[GIMP_LASTVAL_FLOAT].lastval_type            = GIMP_LASTVAL_FLOAT;
    token_tab[GIMP_LASTVAL_DOUBLE].lastval_type           = GIMP_LASTVAL_DOUBLE;
    token_tab[GIMP_LASTVAL_DRAWABLE].lastval_type         = GIMP_LASTVAL_DRAWABLE;
    token_tab[GIMP_LASTVAL_GINTDRAWABLE].lastval_type     = GIMP_LASTVAL_GINTDRAWABLE;
    token_tab[GIMP_LASTVAL_GBOOLEAN].lastval_type         = GIMP_LASTVAL_GBOOLEAN;
    token_tab[GIMP_LASTVAL_ENUM].lastval_type             = GIMP_LASTVAL_ENUM;
    token_tab[GIMP_LASTVAL_GUINT].lastval_type            = GIMP_LASTVAL_GUINT;
    token_tab[GIMP_LASTVAL_GUINT32].lastval_type          = GIMP_LASTVAL_GUINT32;
    token_tab[GIMP_LASTVAL_END].lastval_type              = 0;

    token_table_initialized = TRUE;
  }
}       /* end p_init_token_table */


static gchar *
p_load_lastval_desc_file(const gchar *fname)
{
  FILE  *fp;
  gint32 file_size;
  struct stat stat_buf;
  gchar *file_buff;

  file_size = 0;
  file_buff = NULL;
  /* get filelength */
  if (0 == g_stat(fname, &stat_buf))
  {
    file_size = stat_buf.st_size;
    /* load File into Buffer */
    file_buff = g_malloc0(file_size+1);

    fp = g_fopen(fname, "rb");              /* open read */
    if(fp == NULL)
    {
      printf ("open(read) error on '%s'\n", fname);
      g_free(file_buff);
    }
    fread(file_buff, 1, file_size, fp);
    fclose(fp);
  }
  return (file_buff);
}       /* end p_load_lsatval_desc_file */


static void
p_fwrite_lastvals_desc(FILE *fp, const gchar *keyname, GimpLastvalDescType *lastval_desc_arr, gint32 argc)
{
  p_init_token_table();
  
  if(fp)
  {
    gint32 l_idx;
    gint   l_indent;
    gint   l_col;
    struct      tm *l_t;
    long        l_ti;

    l_indent = 0;
    l_ti = time(0L);          /* Get UNIX time */
    l_t  = localtime(&l_ti);  /* konvert time to tm struct */
    
    fprintf(fp, "\"%s\" ", keyname);
    fprintf(fp, "#- added or changed by GIMP on %04d-%02d-%02d %02d:%02d:%02d\n"
           , l_t->tm_year + 1900
           , l_t->tm_mon + 1
           , l_t->tm_mday
           , l_t->tm_hour
           , l_t->tm_min
           , l_t->tm_sec
           );
    
    for(l_idx=0; l_idx < argc; l_idx++)
    {
      if(lastval_desc_arr[l_idx].lastval_type == GIMP_LASTVAL_STRUCT_END)
      {
         l_indent--;
      }
      for(l_col=0; l_col < l_indent; l_col++)
      {
        fprintf(fp, " ");
      }
      fprintf(fp, "%s;%d;%d;%d;%s\n"
                , token_tab[lastval_desc_arr[l_idx].lastval_type].token
                , (int)lastval_desc_arr[l_idx].offset
                , (int)lastval_desc_arr[l_idx].elem_size
                , (int)lastval_desc_arr[l_idx].iter_flag
                , lastval_desc_arr[l_idx].elem_name
                );
      if(lastval_desc_arr[l_idx].lastval_type == GIMP_LASTVAL_STRUCT_BEGIN)
      {
         l_indent++;
      }
    }
    fprintf(fp, "END;\n");
  }
}       /* end p_fwrite_lastvals_desc */


static gint32
p_fwrite_lines_until_keyname(FILE *fp, const char *keyname, gchar *ptr)
{
   gint32 l_idx;
   gint32 l_lin;
   
   l_idx = 0;
   l_lin = 0;
   if((ptr) && (fp))
   {
      while(ptr[l_idx] != '\0')
      {
        l_lin = l_idx;
        /* skip white space */
        while((ptr[l_idx] == ' ') || (ptr[l_idx] == '\t'))  { l_idx++; }
        
        if(ptr[l_idx] == '"')
        {
          if(strncmp(&ptr[l_idx+1], keyname, strlen(keyname)) == 0)
          {
            l_idx = l_lin;
            break;
          }
        }
        
        /* skip rest of the line */
        while(ptr[l_idx] != '\0')
        {
          if (ptr[l_idx] == '\n')
          {
            l_idx++; 
            break;
          }
          l_idx++;
        }
        
      }
      fwrite(ptr, l_idx, 1, fp);
   }
   return l_idx;
}       /* end p_fwrite_lines_until_keyname */



static void
p_fwrite_lines_remaining_without_keyname(FILE *fp, const char *keyname, gchar *ptr)
{
   gint32 l_idx;
   gint32 l_lin;
   gboolean l_write_flag;
   
   l_idx = 0;
   l_lin = 0;
   l_write_flag = TRUE;
   if((ptr) && (fp))
   {
      while(ptr[l_idx] != '\0')
      {
        l_lin = l_idx;
        /* skip white space */
        while((ptr[l_idx] == ' ') || (ptr[l_idx] == '\t'))  { l_idx++; }
        
        if(ptr[l_idx] == '"')
        {
          if(strncmp(&ptr[l_idx+1], keyname, strlen(keyname)) == 0)
          {
            l_write_flag = FALSE;  /* dont write matching description block(s) */
          }
          else
          {
            l_write_flag = TRUE;   /* write all non-matching description blocks */
          }
        }
        
        /* skip rest of the line */
        while(ptr[l_idx] != '\0')
        {
          if (ptr[l_idx] == '\n')
          {
            l_idx++; 
            break;
          }
          l_idx++;
        }
    
        if((l_write_flag) && (l_idx > l_lin))
        {
          /* write one line to file */
          fwrite(&ptr[l_lin], l_idx - l_lin, 1, fp);
        }        
        if(strncmp(&ptr[l_lin], "END;", 4) == 0)
        {
            l_write_flag = TRUE;   /* enable write when block has ended */
        }
      }
   }
}       /* end p_fwrite_lines_remaining_without_keyname */


static void
p_lastvals_register_persistent(const gchar *keyname, GimpLastvalDescType *lastval_desc_arr, gint32 argc)
{
  FILE  *fp;
  gchar *fname;
  gchar *file_buff;
  gchar *ptr;
  gint32 bytes_written;
  
  fname = gimp_lastval_desc_filename();
  file_buff = p_load_lastval_desc_file(fname);
  
  ptr = file_buff;

  /* rewrite file (replacing or adding  the structure description block for keyname) */
  fp = g_fopen(fname, "w");
  if(fp)
  {
    /* write all lines until 1.st description line matching keyname */
    bytes_written = p_fwrite_lines_until_keyname(fp, keyname, ptr);
    
    if(bytes_written > 0)
    {
      ptr += bytes_written;
    }
    else
    {
      /* start with comment header if file was empty */
      fprintf(fp, "# Descriptionfile LAST_VALUES buffer structure (type;offset;size;iterflag,name)\n");
      fprintf(fp, "#\n");
      fprintf(fp, "# this file is rewritten each time when a procedure registers\n");
      fprintf(fp, "# the LAST_VALUES structure. (typical at 1.st gimp startup\n");
      fprintf(fp, "# or after installation of new plug-ins)\n");
      fprintf(fp, "#\n");
    }

    /* append description lines for keyname (or replace 1.st occurance) */
    p_fwrite_lastvals_desc(fp, keyname, lastval_desc_arr, argc);

    /* copy the rest without description block(s) for keyname */
    p_fwrite_lines_remaining_without_keyname(fp, keyname, ptr);
    
    fclose(fp);
  }
  else
  {
    printf("p_lastvals_register_persistent: error at open write file: %s\n", fname);
  }
  g_free(fname);
}       /* end p_lastvals_register_persistent */
