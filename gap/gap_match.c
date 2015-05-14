/* gap_match.c
 * 1998.10.14 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains:
 * layername and layerstacknumber matching procedures
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
 * gimp   1.3.20a;   2003/09/14  hof: gap_match_name increased limit of 256 bytes to 2048
 * gimp   1.1.29b;   2000/11/30  hof: used g_snprintf
 * version 0.97.00  1998.10.14  hof: - created module
 */
#include "config.h"

/* SYSTEM (UNIX) includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* GIMP includes */
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_match.h"

/* ---------------------------------
 * gap_match_string_is_empty
 * ---------------------------------
 */
int
gap_match_string_is_empty (const char *str)
{
  if(str == NULL)  return(TRUE);
  if(*str == '\0') return(TRUE);


  if(g_utf8_validate(str, -1, NULL))
  {
    while(*str != '\0')
    {
      gunichar uni_char;
      
      uni_char = g_utf8_get_char(str);
      
      if(!g_unichar_isspace(uni_char)) return(FALSE);
      str = g_utf8_find_next_char(str, NULL);
    }
    return(TRUE);
  }
  
  while(*str != '\0')
  {
    if(*str != ' ') return(FALSE);
    str++;
  }

  return(TRUE);
}  /* end gap_match_string_is_empty */


/* ---------------------------------
 * gap_match_substitute_framenr
 * ---------------------------------
 *    copy new_layername to buffer
 *    and substitute [####] by curr frame number
 */
void
gap_match_substitute_framenr (char *buffer, int buff_len, char *new_layername, long curr)
{
  int l_idx;
  int l_digits;
  int l_cpy;
  char  l_fmt_str[21];
  gboolean    utf8_compliant;
  const char *src_ptr;

  l_fmt_str[0] = '%';
  l_fmt_str[1] = '0';
  l_digits = 0;

  l_idx = 0;
  if(new_layername != NULL)
  {
    utf8_compliant = g_utf8_validate(new_layername, -1, NULL);

    while((l_idx < (buff_len-1)) && (*new_layername != '\0') )
    {
      l_cpy    = 1;
      switch(*new_layername)
      {
        case '[':
          if((new_layername[1] == '#') && (l_digits == 0))
          {
            l_cpy    = 0;
            l_digits = 1;
          }
          break;
        case '#':
          if(l_digits > 0)
          {
            l_cpy    = 0;
            l_digits++;
          }
          break;
        case ']':
          if(l_digits > 0)
          {
            l_digits--;
            g_snprintf(&l_fmt_str[2], sizeof(l_fmt_str) -2,  "%dd", l_digits);
            g_snprintf(&buffer[l_idx], buff_len - l_idx, l_fmt_str, (int)curr);
            l_idx += l_digits;
            l_digits = 0;
            l_cpy    = 0;
          }
          break;
        default:
          l_digits = 0;
          break;
      }
      
      src_ptr = new_layername;
      
      if (utf8_compliant)
      {
        new_layername = g_utf8_find_next_char(new_layername, NULL);
      }
      else
      {
        new_layername++;
      }

      if(l_cpy != 0)
      {
        while(src_ptr != new_layername)
        {
          buffer[l_idx] = (*src_ptr);
          src_ptr++;
          l_idx++;
        }
      }



    }
  }

  buffer[l_idx] = '\0';
}       /* end gap_match_substitute_framenr */


/* ---------------------------------
 * p_ascii_strup
 * ---------------------------------
 */
static gchar*
p_ascii_strup(const gchar *input_string)
{
  gchar *output_str;
  
  output_str = NULL;
  if(input_string != NULL)
  {
     gchar *str;
     
     output_str = g_strdup(input_string);
     str = output_str;
     while(*str != '\0')
     {
       *str = g_ascii_toupper(*str);
       str++;
     }
  }
  return (output_str);
}  /* end p_ascii_strup */


/* ---------------------------------
 * gap_match_number
 * ---------------------------------
 * match layer_idx (int) with pattern
 * pattern contains a list like that:
 *  "0, 3-4, 7, 10"
 */
int 
gap_match_number(gint32 layer_idx, const char *pattern)
{
   char        l_digit_buff[128];
   const char *l_ptr;
   int         l_idx;
   gint32      l_num, l_range_start;
   gboolean    utf8_compliant;

   utf8_compliant = g_utf8_validate(pattern, -1, NULL);
   l_idx = 0;
   l_num = -1; l_range_start = -1;
   l_ptr = pattern; 
   while(1 == 1)
   {
      if(g_ascii_isdigit(*l_ptr))
      {
         l_digit_buff[l_idx] = *l_ptr;   /* collect digits here */
         l_idx++;
      }
      else
      {
         if(l_idx > 0)
         {
            /* now we are one character past a number */
            l_digit_buff[l_idx] = '\0';
            l_num = atol(l_digit_buff);  /* scan the number */

            if(l_num == layer_idx)
            {
               return(TRUE);             /* matches number exactly */
            }

            if((l_range_start >= 0)
            && (layer_idx >= l_range_start) && (layer_idx <= l_num ))
            {
               return(TRUE);             /* matches given range */
            }

            l_range_start = -1;     /* disable number for opening a range */
            l_idx = 0;
          }

          switch(*l_ptr)
          {
            case '\0':
               return (FALSE);
               break;
            case ' ':
            case '\t':
               break;
            case ',':
               l_num = -1;              /* disable number for opening a range */
               break;
            case '-':
                 if (l_num >= 0)
                 {
                    l_range_start = l_num;  /* prev. number opens a range */
                 }
                 break;
            default:
               /* found illegal characters */
               /* return (FALSE); */
               l_num = -1;             /* disable number for opening a range */
               l_range_start = -1;     /* disable number for opening a range */

               break;
          }
      }
      
      if(utf8_compliant)
      {
        l_ptr = g_utf8_find_next_char(l_ptr, NULL);
      }
      else
      {
        l_ptr++;
      }
   }   /* end while */

   return(FALSE);
}       /* end gap_match_number */


/* ---------------------------------
 * p_match_name
 * ---------------------------------
 * simple string matching.
 * since this procedure checks only for equal strings
 * it uses strcmp and strncmp regerdless if utf8 complient or not.
 * if dealing with utf8 compliant strings, iterating 
 * takes care that utf8 character can have more than one byte.
 *
 * returns FALSE for non matching string
 */
static int 
p_match_name(const char *layername, const char *pattern, gint32 mode, gboolean utf8_compliant)
{
   int l_llen;
   int l_plen;
   const char *uni_ptr;

   switch (mode)
   {
      case GAP_MTCH_EQUAL:
           if (0 == strcmp(layername, pattern))
           {
             return(TRUE);
           }
           break;
      case GAP_MTCH_START:
           l_plen = strlen(pattern);
           if (0 == strncmp(layername, pattern, l_plen))
           {
             return(TRUE);
           }
           break;
      case GAP_MTCH_END:
      case GAP_MTCH_ANYWHERE:
           l_llen = strlen(layername);
           l_plen = strlen(pattern);
           uni_ptr = layername;
           while (l_llen >= l_plen)
           {
             /* printf("GAP_MTCH :%s: llen:%d \n", uni_ptr, (int)l_llen); */
             
             if((l_llen == l_plen) || (mode == GAP_MTCH_ANYWHERE))
             {
               if(0 == strncmp(uni_ptr, pattern, l_plen))
               {
                 return(TRUE);
               }
             }

             if(utf8_compliant)
             {
               uni_ptr = g_utf8_find_next_char(uni_ptr, NULL);
             }
             else
             {
               uni_ptr++;
             }
             l_llen = strlen(uni_ptr);
           }
           break;
      default:
           break;

   }

   return (FALSE);

}  /* end p_match_name */


/* ---------------------------------
 * gap_match_name
 * ---------------------------------
 * simple stringmatching without wildcards
 * return TRUE if layername matches pattern according to specified mode
 * NULL pointers never match (even if compared with NULL)
 */
int 
gap_match_name(const char *layername, const char *pattern, gint32 mode, gint32 case_sensitive)
{
   int l_rc;
   const gchar *l_name_ptr;
   const gchar *l_patt_ptr;
   gchar  *l_name_buff;
   gchar  *l_patt_buff;
   gboolean utf8_compliant;

   if(pattern == NULL)   return (FALSE);
   if(layername == NULL) return (FALSE);

   l_name_buff = NULL;
   l_patt_buff = NULL;


   /* check utf8 compliance for both layername and pattern */
   utf8_compliant = FALSE;
   if(g_utf8_validate(layername, -1, NULL))
   {
     utf8_compliant = g_utf8_validate(pattern, -1, NULL);
   }


   if(case_sensitive)
   {
     /* case sensitive can compare on the originals */
     l_name_ptr = layername;
     l_patt_ptr = pattern;
   }
   else
   {
     if(utf8_compliant)
     {
       /* ignore case by converting everything to UPPER before compare */
       l_name_buff = g_utf8_strup(layername, -1);
       l_patt_buff = g_utf8_strup(pattern, -1);

     }
     else
     {
       l_name_buff = p_ascii_strup(layername);
       l_patt_buff = p_ascii_strup(pattern);
     }
     l_name_ptr = l_name_buff;
     l_patt_ptr = l_patt_buff;
   }

   l_rc = p_match_name(l_name_ptr, l_patt_ptr, mode, utf8_compliant);
   
   if(l_name_buff)
   {
     g_free(l_name_buff);
   }
   
   if(l_patt_buff)
   {
     g_free(l_patt_buff);
   }


   return (l_rc);

}  /* end gap_match_name */


/* ---------------------------------
 * gap_match_layer
 * ---------------------------------
 */
int 
gap_match_layer(gint32 layer_idx, const char *layername, const char *pattern,
                  gint32 mode, gint32 case_sensitive, gint32 invert,
                  gint nlayers, gint32 layer_id)
{
   int l_rc;
   switch(mode)
   {
     case GAP_MTCH_NUMBERLIST:
          l_rc = gap_match_number(layer_idx, pattern);
          break;
     case GAP_MTCH_INV_NUMBERLIST:
          l_rc = gap_match_number((nlayers -1) - layer_idx, pattern);
          break;
     case GAP_MTCH_ALL_VISIBLE:
          l_rc = gimp_drawable_get_visible(layer_id);
          break;
     default:
          l_rc = gap_match_name(layername, pattern, mode, case_sensitive);
          break;
   }

   if(invert == TRUE)
   {
      if(l_rc == FALSE)  { return(TRUE); }
      return(FALSE);
   }
   return (l_rc);
}  /* end gap_match_layer */
