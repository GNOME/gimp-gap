/*  gap_val_file.c
 *
 *  This module handles GAP key/value paramterfiles.
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
 * version 2.1.0a;  2007/05/07  hof: created (splitted of generic key/value parts gap_vin) 
 */
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include <glib/gstdio.h>

/* GIMP includes */
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_val_file.h"
extern int gap_debug;



/* --------------------------
 * gap_val_free_keylist
 * --------------------------
 */
void
gap_val_free_keylist(GapValKeyList *keylist)
{
  GapValKeyList *keyptr;
  
  while(keylist)
  {
    keyptr = (GapValKeyList *)keylist->next;
    g_free(keylist);
    keylist = keyptr;
  }

}  /* end gap_val_free_keylist */


/* --------------------------
 * gap_val_new_keylist
 * --------------------------
 */
GapValKeyList*
gap_val_new_keylist(void)
{
  GapValKeyList *keyptr;
  
  keyptr = g_malloc0(sizeof(GapValKeyList));
  keyptr->keyword[0] = '\0';
  keyptr->comment[0] = '\0';
  keyptr->len = 0;
  keyptr->val_ptr = NULL;
  keyptr->dataype = GAP_VAL_DUMMY;
  keyptr->done_flag = FALSE;
  keyptr->next = NULL;
  
  return(keyptr);

}  /* end gap_val_new_keylist */


/* --------------------------
 * gap_val_set_keyword
 * --------------------------
 * create a new element, init with the passed args
 * and add to end of the passed keylist.
 *
 * keylist must contain at least one element.
 * (if this is a dummy it will be replaced)
 */
void
gap_val_set_keyword(GapValKeyList *keylist
             , const gchar *keyword
             , gpointer val_ptr
             , GapValDataType dataype
             , gint32 len
             , const gchar *comment
             )
{
  GapValKeyList *keyptr;
  
  if(keylist == NULL)
  {
    printf ("** INTERNAL ERROR gap_val_set_keyword was called with keylist == NULL\n");
    return;
  }
  
  if(keyword == NULL)
  {
    printf ("** INTERNAL ERROR gap_val_set_keyword was called with keyword == NULL\n");
    return;
  }

  if(keylist->dataype == GAP_VAL_DUMMY)
  {
    /* if the 1.st element is a dummy
     * we replace the content rather than creating a new element
     */
    keyptr = keylist;
  }
  else
  {
    keyptr = gap_val_new_keylist();
    
    /* add new element to end of keylist */  
    while(keylist->next)
    {
      keylist = (GapValKeyList *)keylist->next;
    }
    keylist->next = keyptr;
  }
  
  /* copy the passed args into keyptr element */
  g_snprintf(&keyptr->keyword[0], sizeof(keyptr->keyword), "%s", keyword);
  if(comment)
  {
    g_snprintf(&keyptr->comment[0], sizeof(keyptr->comment), "%s", comment);
  }
  keyptr->len = len;
  keyptr->val_ptr = val_ptr;
  keyptr->dataype = dataype;
  
}  /* end gap_val_set_keyword */


/* ---------------------------
 * gap_val_free_textfile_lines
 * ---------------------------
 */
void
gap_val_free_textfile_lines(GapValTextFileLines *txf_ptr_root)
{
  GapValTextFileLines *txf_ptr;
  GapValTextFileLines *txf_ptr_next;


  txf_ptr_next = NULL;
  for(txf_ptr = txf_ptr_root; txf_ptr != NULL; txf_ptr = txf_ptr_next)
  {
     txf_ptr_next = (GapValTextFileLines *) txf_ptr->next;
     g_free(txf_ptr->line);
     g_free(txf_ptr);
  }
}  /* end gap_val_free_textfile_lines */


/* --------------------------
 * gap_val_load_textfile
 * --------------------------
 * load all lines from a textfile
 * into a GapValTextFileLines list structure 
 * and return root pointer of this list.
 * return NULL if file not found or empty.
 */
GapValTextFileLines *
gap_val_load_textfile(const char *filename)
{
  FILE *l_fp;
  GapValTextFileLines *txf_ptr;
  GapValTextFileLines *txf_ptr_prev;
  GapValTextFileLines *txf_ptr_root;
  char         l_buf[GAP_VAL_MAX_BYTES_PER_LINE +1];
  int   line_nr;
  
  line_nr = 0;
  txf_ptr_prev = NULL;
  txf_ptr_root = NULL;
  l_fp = g_fopen(filename, "r");
  if(l_fp)
  {
    while(NULL != fgets(l_buf, GAP_VAL_MAX_BYTES_PER_LINE, l_fp))
    {
      line_nr++;
      txf_ptr = g_malloc0(sizeof(GapValTextFileLines));
      txf_ptr->line = g_strdup(l_buf);
      txf_ptr->line_nr=line_nr;
      txf_ptr->next = NULL;
      
      if(txf_ptr_prev == NULL)
      {
        txf_ptr_root = txf_ptr;
      }
      else
      {
        txf_ptr_prev->next = txf_ptr;
      }
      txf_ptr_prev = txf_ptr;
    }
    fclose(l_fp);
  }

  return(txf_ptr_root);
}  /* end gap_val_load_textfile */



/* ---------------------------
 * p_write_keylist_value
 * ---------------------------
 * write Key/value entry (one line) to file,
 * value format depends on datatype
 *   (keyname value) #comment
 */
static void
p_write_keylist_value(FILE *fp, GapValKeyList *keyptr, const char *term_str)
{
  static char *termbuf = "\0";
  const char *term_ptr;
  
  
  term_ptr = termbuf;
  if(term_str)
  {
    term_ptr = term_str;
  }
  
  switch(keyptr->dataype)
  {
    case GAP_VAL_GINT:
      {
        gint *val_ptr;
      
        val_ptr = (gint *)keyptr->val_ptr;
        fprintf(fp, "%s%d%s %s\n"
               , keyptr->keyword   /* "(keyword " */
               , (int)*val_ptr          /* value */
               , term_ptr
               , keyptr->comment
               );
      }
      break;
    case GAP_VAL_GINT32:
      {
        gint32 *val_ptr;
      
        val_ptr = (gint32 *)keyptr->val_ptr;
        fprintf(fp, "%s%d%s %s\n"
               , keyptr->keyword   /* "(keyword " */
               , (int)*val_ptr          /* value */
               , term_ptr
               , keyptr->comment
               );
      }
      break;
    case GAP_VAL_GINT64:
      {
        gint64 *val_ptr;
        char   *l_str;


        val_ptr = (gint64 *)keyptr->val_ptr;
        l_str = g_strdup_printf("%s%lld%s %s"
               , keyptr->keyword   /* "(keyword " */
               , *val_ptr          /* value */
               , term_ptr
               , keyptr->comment
               );
        fprintf(fp, "%s\n", l_str);
        g_free(l_str);

      }
      break;
    case GAP_VAL_GDOUBLE:
      {
        gdouble *val_ptr;
        gchar l_dbl_str[G_ASCII_DTOSTR_BUF_SIZE];
      
        val_ptr = (gdouble *)keyptr->val_ptr;
        /* setlocale independent float string */
        g_ascii_dtostr(&l_dbl_str[0]
                     ,G_ASCII_DTOSTR_BUF_SIZE
                     ,*val_ptr
                     );
     
        fprintf(fp, "%s%s%s %s\n"
               , keyptr->keyword   /* "(keyword " */
               , l_dbl_str         /* value */
               , term_ptr
               , keyptr->comment
               );
      }
      break;
    case GAP_VAL_GBOOLEAN:
      {
        gboolean *val_ptr;

        val_ptr = (gboolean *)keyptr->val_ptr;
        if(*val_ptr)
        {
          fprintf(fp, "%syes%s %s\n"
               , keyptr->keyword   /* "(keyword " */
               , term_ptr
               , keyptr->comment
               );
        }
        else
        {
          fprintf(fp, "%sno%s %s\n"
               , keyptr->keyword   /* "(keyword " */
               , term_ptr
               , keyptr->comment
               );
        }
      }
      break;
    case GAP_VAL_G32BOOLEAN:
      {
        gint32 *val_ptr;

        val_ptr = (gint32 *)keyptr->val_ptr;
        if((*val_ptr == TRUE) || (*val_ptr == 1))
        {
          fprintf(fp, "%syes%s %s\n"
               , keyptr->keyword   /* "(keyword " */
               , term_ptr
               , keyptr->comment
               );
        }
        else
        {
          if((*val_ptr == FALSE) || (*val_ptr == 0))
          {
            fprintf(fp, "%sno%s %s\n"
               , keyptr->keyword   /* "(keyword " */
               , term_ptr
               , keyptr->comment
               );
          }
          else
          {
            fprintf(fp, "%s%d%s %s\n"
               , keyptr->keyword   /* "(keyword " */
               , (int)*val_ptr     /* value */
               , term_ptr
               , keyptr->comment
               );
          }
            }
      }
      break;
    case GAP_VAL_STRING:
      {
        gchar  *val_ptr;
        gint   idx;

        val_ptr = (gchar *)keyptr->val_ptr;
        fprintf(fp, "%s \""
               , keyptr->keyword   /* "(keyword " */
               );

        for(idx=0; idx < keyptr->len; idx++)
        {
          if((val_ptr[idx] == '\0')
          || (val_ptr[idx] == '\n'))
          {
            break;
          }
          if((val_ptr[idx] == '\\')
          || (val_ptr[idx] == '"'))
          {
            fprintf(fp, "\\%c", val_ptr[idx]);
          }
          else
          {
            fprintf(fp, "%c", val_ptr[idx]);
          }
        }
               
        fprintf(fp, "\"%s %s\n"
               , term_ptr
               , keyptr->comment
               );
        
      }
      break;
    default:
      break;
  }  /* end switch */

}  /* end p_write_keylist_value */


/* --------------------------
 * gap_val_rewrite_file
 * --------------------------
 * (re)write the file
 * only the values for the keywords in the passed keylist
 * are created (or replaced) in the file.
 * all other lines are left unchanged.
 * if the file is empty a header  is added.
 *  (this is only done if the header text is specified
 *   in the hdr_text parameter.
 *   pass NULL if you dont want add header text
 *  )
 * return: 0 if file could be written, -1 on error
 */
int
gap_val_rewrite_file(GapValKeyList *keylist, const char *filename, const char *hdr_text, const char *term_str)
{
  FILE *l_fp;
  GapValTextFileLines *txf_ptr_root;
  GapValTextFileLines *txf_ptr;
  GapValKeyList *keyptr;
  int   l_rc;
  int   l_len;
   
  l_rc = -1;

  for(keyptr=keylist; keyptr != NULL; keyptr = (GapValKeyList*)keyptr->next)
  {
     keyptr->done_flag = FALSE;
  }   /* end for keylist loop */

  if(filename)
  {
      txf_ptr_root = gap_val_load_textfile(filename);
  
      l_fp = g_fopen(filename, "w");
      if(l_fp)
      {
         for(txf_ptr = txf_ptr_root; txf_ptr != NULL; txf_ptr = (GapValTextFileLines *) txf_ptr->next)
         {
           gboolean line_done;
           
           line_done = FALSE;
           for(keyptr=keylist; keyptr != NULL; keyptr = (GapValKeyList*)keyptr->next)
           {
             l_len = strlen(keyptr->keyword);
             if(strncmp(txf_ptr->line, keyptr->keyword, l_len) == 0)
             {
               /* replace the existing line (same key, new value) */
               p_write_keylist_value(l_fp, keyptr, term_str);
               keyptr->done_flag = TRUE;
               line_done = TRUE;
               break;
             }
           }   /* end for keylist loop */
           
           if(line_done == FALSE)
           {
              /* 1:1 copy of lines with unhandled keywords
               * (and add newline if required)
               * for keeping comment lines
               * and for compatibility with future keywords
               */
              l_len = strlen(txf_ptr->line);
              if(txf_ptr->line[MAX(0,l_len-1)] == '\n')
              {
                 fprintf(l_fp, "%s", txf_ptr->line);
              }
              else
              {
                 fprintf(l_fp, "%s\n", txf_ptr->line);
              }
           }
           
         }  /* end loop for each textline */
         
         if(txf_ptr_root)
         {
           gap_val_free_textfile_lines(txf_ptr_root);
         }
         else
         {
           if(hdr_text)
           {
             /* write header if file was empty or not existent */
             fprintf(l_fp, "%s\n", hdr_text);
           }
         }
         
         /* write the unhandled key/values (where key was not found in the file before) */
         for(keyptr=keylist; keyptr != NULL; keyptr = (GapValKeyList*)keyptr->next)
         {
           if(keyptr->done_flag == FALSE)
           {
             p_write_keylist_value(l_fp, keyptr, term_str);
             keyptr->done_flag = TRUE;
           }
         }   /* end for keylist loop */

         fclose(l_fp);
         l_rc = 0;
       }
  }

  return(l_rc);
}  /* end gap_val_rewrite_file */


/* --------------------------
 * gap_val_scann_filevalues
 * --------------------------
 * get values for all specified keynames in the keylist from file
 * returns the number of recognized keynames (== scanned values)
 */
int
gap_val_scann_filevalues(GapValKeyList *keylist, const char *filename)
{
  GapValKeyList *keyptr;
  GapValTextFileLines *txf_ptr_root;
  GapValTextFileLines *txf_ptr;
  int   l_len;
  int   l_cnt_keys;
  
  l_cnt_keys = 0;
  if(filename)
  {
      txf_ptr_root = gap_val_load_textfile(filename);

      for(txf_ptr = txf_ptr_root; txf_ptr != NULL; txf_ptr = (GapValTextFileLines *) txf_ptr->next)
      {
          for(keyptr=keylist; keyptr != NULL; keyptr = (GapValKeyList*)keyptr->next)
          {
             l_len = strlen(keyptr->keyword);
             if(strncmp(txf_ptr->line, keyptr->keyword, l_len) == 0)
             {
               l_cnt_keys++;
               switch(keyptr->dataype)
               {
                 case GAP_VAL_GINT:
                   {
                      gint *val_ptr;
                      
                      val_ptr = (gint *)keyptr->val_ptr;
                      *val_ptr = atol(&txf_ptr->line[l_len]);
                   }
                   break;
                 case GAP_VAL_GINT32:
                   {
                      gint32 *val_ptr;
                      
                      val_ptr = (gint32 *)keyptr->val_ptr;
                      *val_ptr = atol(&txf_ptr->line[l_len]);
                   }
                   break;
                 case GAP_VAL_GINT64:
                   {
                      gint64 *val_ptr;
                      
                      val_ptr = (gint64 *)keyptr->val_ptr;
                      *val_ptr = atoll(&txf_ptr->line[l_len]);
                   }
                   break;
                 case GAP_VAL_GDOUBLE:
                   {
                      gdouble *val_ptr;
                    
                      val_ptr = (gdouble *)keyptr->val_ptr;
                      /* setlocale independent string to double converion */
                      *val_ptr = g_ascii_strtod(&txf_ptr->line[l_len], NULL);
                   }
                   break;
                 case GAP_VAL_GBOOLEAN:
                   {
                      gboolean *val_ptr;
                      
                      val_ptr = (gboolean *)keyptr->val_ptr;
                      if((txf_ptr->line[l_len] == 'N')
                      || (txf_ptr->line[l_len] == 'n')
                      || (txf_ptr->line[l_len] == '0'))
                      {
                        *val_ptr = FALSE;
                      }
                      else
                      {
                        *val_ptr = TRUE;
                      }
                   }
                   break;
                 case GAP_VAL_G32BOOLEAN:
                   {
                      gint32 *val_ptr;

                      val_ptr = (gint32 *)keyptr->val_ptr;
                      if((txf_ptr->line[l_len] == 'N')
                      || (txf_ptr->line[l_len] == 'n'))
                      {
                        *val_ptr = 0;
                      }
                      else
                      {
                        if((txf_ptr->line[l_len] == 'Y')
                        || (txf_ptr->line[l_len] == 'y'))
                        {
                          *val_ptr = 1;
                        }
                        else
                        {
                          *val_ptr = atol(&txf_ptr->line[l_len]);
                        }
                      }
                   }
                   break;
                 case GAP_VAL_STRING:
                   {
                      gchar *val_ptr;
                      gboolean esc_flag;
                      gint32   l_idx;
                      
                      val_ptr = (gchar *)keyptr->val_ptr;
                      while(txf_ptr->line[l_len] == ' ')
                      {
                        l_len++;  /* skip spaces between keyword and starting quote */
                      }
                      if(txf_ptr->line[l_len] == '"')
                      {
                        l_len++;  /* skip starting quote */
                        esc_flag = FALSE;
                        for(l_idx=0; l_idx < MIN(4000-l_len, keyptr->len); l_idx++)
                        {
                          if((txf_ptr->line[l_len+l_idx] == '\n')
                          || (txf_ptr->line[l_len+l_idx] == '\0'))
                          {
                            break;
                          }
                          if((txf_ptr->line[l_len+l_idx] == '\\')
                          && (esc_flag == FALSE))
                          {
                            esc_flag = TRUE;
                            continue;
                          }
                          if((txf_ptr->line[l_len+l_idx] == '"')
                          && (esc_flag == FALSE))
                          {
                            break;
                          }
                          *(val_ptr++) = txf_ptr->line[l_len+l_idx];
                          esc_flag = FALSE;
                          
                        }
                      }
                      *val_ptr = '\0';
                   }
                   break;
                 default:
                   break;
               }
             
               break;
             }
          }  /* end for keylist loop */
      } /* end for text lines scan loop */
      if(txf_ptr_root)
      {
        gap_val_free_textfile_lines(txf_ptr_root);
      }

  }

  return(l_cnt_keys);
}  /* end gap_val_scann_filevalues */
