/* gap_xml_util.c
 * 2011.03.09 hof (Wolfgang Hofer)
 *
 * This module contains procedures to parse basic datatypes from string and write to file.
 * Intended for use in parser functions called by GMarkupParser
 * to read data from attributes in XML elements.
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
 * 2011.03.09  hof: created.
 */
 
#include "config.h"
#include "gap_base.h"
#include "gap_xml_util.h"

/* --------------------------------------
 * gap_xml_parse_value_gdouble
 * --------------------------------------
 */
gboolean
gap_xml_parse_value_gdouble(const gchar *attribute_value, gdouble *valDestPtr)
{
  gchar *endptr;
  
  *valDestPtr = g_ascii_strtod(attribute_value, &endptr);
  if(attribute_value == endptr)
  {
    /* pointer was not advanced (no float number could be scanned */
    return (FALSE);
  }
  return(TRUE);
}

/* --------------------------------------
 * gap_xml_parse_value_gint32
 * --------------------------------------
 */
gboolean
gap_xml_parse_value_gint32(const gchar *attribute_value, gint32 *valDestPtr)
{
  gint64 value64;
  gchar *endptr;
  
  value64 = g_ascii_strtoll(attribute_value, &endptr, 10);
  if(attribute_value == endptr)
  {
    /* pointer was not advanced (no int number could be scanned */
    return (FALSE);
  }
  *valDestPtr = value64;
  return (TRUE);
}

/* --------------------------------------
 * gap_xml_parse_value_gint
 * --------------------------------------
 */
gboolean
gap_xml_parse_value_gint(const gchar *attribute_value, gint *valDestPtr)
{
  gint64 value64;
  gchar *endptr;
  
  value64 = g_ascii_strtoll(attribute_value, &endptr, 10);
  if(attribute_value == endptr)
  {
    /* pointer was not advanced (no int number could be scanned */
    return (FALSE);
  }
  *valDestPtr = value64;
  return (TRUE);
}



/* --------------------------------------
 * gap_xml_parse_value_gboolean
 * --------------------------------------
 */
gboolean
gap_xml_parse_value_gboolean(const gchar *attribute_value, gboolean *valDestPtr)
{
  gboolean isOk = TRUE;

  *valDestPtr = FALSE;
  if (strcmp("TRUE", attribute_value) == 0)
  {
    *valDestPtr = TRUE;
  }
  else if(strcmp("FALSE", attribute_value) == 0)
  {
    *valDestPtr = FALSE;
  }
  else
  {
    gint intVal;
    
    isOk = gap_xml_parse_value_gint(attribute_value, &intVal);
    if(isOk)
    {
      *valDestPtr = (intVal != 0);
    }
  }
  
  return (isOk);
  
}  /* end gap_xml_parse_value_gboolean */



/* --------------------------------------
 * gap_xml_parse_value_gboolean_as_gint
 * --------------------------------------
 */
gboolean
gap_xml_parse_value_gboolean_as_gint(const gchar *attribute_value, gint *valDestPtr)
{
  gboolean val;
  gboolean isOk;
  
  val = FALSE;
  isOk = gap_xml_parse_value_gboolean(attribute_value, &val);
  if(val)
  {
    *valDestPtr = 1;
  }
  else
  {
    *valDestPtr = 0;
  }
    
  return (isOk);

}  /* end gap_xml_parse_value_gboolean_as_gint */


/* --------------------------------------
 * gap_xml_parse_EnumValue_as_gint
 * --------------------------------------
 */
gboolean
gap_xml_parse_EnumValue_as_gint(const gchar *attribute_value, gint *valDestPtr,
const GEnumValue *enumValuesTable)
{
  gboolean isOk = FALSE;
  gint     ii;

  for(ii=0; enumValuesTable[ii].value_name != NULL; ii++)
  {
    if (strcmp(enumValuesTable[ii].value_name, attribute_value) == 0)
    {
      *valDestPtr = enumValuesTable[ii].value;
      isOk = TRUE;
      break;
    }
    if(enumValuesTable[ii].value_nick != NULL)
    {
      if (strcmp(enumValuesTable[ii].value_nick, attribute_value) == 0)
      {
        *valDestPtr = enumValuesTable[ii].value;
        isOk = TRUE;
        break;
      }
    }
  }
  
  return (isOk);
  
}  /* end gap_xml_parse_EnumValue_as_gint */


/* --------------------------------------
 * gap_xml_parse_value_utf8_string
 * --------------------------------------
 */
gboolean
gap_xml_parse_value_utf8_string(const gchar *attribute_value, gchar **valDestPtr)
{
  if(*valDestPtr != NULL)
  {
    g_free(*valDestPtr);
    *valDestPtr = NULL;
  }
  if(attribute_value != NULL)
  {
    gboolean    utf8_compliant;

    utf8_compliant = g_utf8_validate(attribute_value, -1, NULL);
    if(utf8_compliant)
    {
       *valDestPtr = g_strdup(attribute_value);
       return (TRUE);
    }
    
  }
  return (FALSE);
}











/* --------------------------------------
 * gap_xml_write_gboolean_value
 * --------------------------------------
 */
void
gap_xml_write_gboolean_value(FILE *fp, const char *name, gboolean value)
{
  if(value)
  {
    fprintf(fp, "%s=\"TRUE\" ", name);
  }
  else
  {
    fprintf(fp, "%s=\"FALSE\" ", name);
  }
}

/* --------------------------------------
 * gap_xml_write_gint_as_gboolean_value
 * --------------------------------------
 */
void
gap_xml_write_gint_as_gboolean_value(FILE *fp, const char *name, gint value)
{
  if(value)
  {
    fprintf(fp, "%s=\"TRUE\" ", name);
  }
  else
  {
    fprintf(fp, "%s=\"FALSE\" ", name);
  }
}

/* --------------------------------------
 * gap_xml_write_int_value
 * --------------------------------------
 */
void
gap_xml_write_int_value(FILE *fp, const char *name, gint32 value)
{
  fprintf(fp, "%s=\"%d\" ", name, (int)value);
}

/* --------------------------------------
 * gap_xml_write_gdouble_value
 * --------------------------------------
 */
void
gap_xml_write_gdouble_value(FILE *fp, const char *name, gdouble value, gint digits, gint precision_digits)
{
  fprintf(fp, "%s=\"", name);
  gap_base_fprintf_gdouble(fp, value, digits, precision_digits, "");
  fprintf(fp, "\" ");
}


/* --------------------------------------
 * gap_xml_write_string_value
 * --------------------------------------
 * write escaped string value.
 * Note that utf8 compliant strings are written unchanged
 * but utf8 non compliant strings are changed where all 
 * characters that are not 7-bit ascii encoded will be replaced by '_'
 * (to force utf8 compatibility)
 */
void
gap_xml_write_string_value(FILE *fp, const char *name, const char *value)
{
  gchar *utf8String;
  gchar *escapedUtf8String;
  gboolean    utf8_compliant;


  if(value == NULL)
  {
    fprintf(fp, "%s=\"\" ", name);
    return;
  }

  utf8_compliant = g_utf8_validate(value, -1, NULL);
  if(utf8_compliant)
  {
    utf8String = g_strdup(value);
  }
  else
  {
    gint ii;
    gint len;
    
    len = strlen(value);
    
    utf8String = g_malloc(len +1);
    
    /* copy 7-bit ascii characters and replace other characters by "_" */
    for(ii=0; ii < len; ii++)
    {
      if(value[ii] <= 0x7f)
      {
        utf8String[ii] = value[ii];
      }
      else
      {
        utf8String[ii] = '_';
      }
    }
    utf8String[ii] = '\0';
  }
  
  escapedUtf8String = g_markup_printf_escaped("%s", utf8String);

  fprintf(fp, "%s=\"%s\" ", name, escapedUtf8String);
  
  g_free(utf8String);
  g_free(escapedUtf8String);

}


/* --------------------------------------
 * gap_xml_write_EnumValue
 * --------------------------------------
 */
void
gap_xml_write_EnumValue(FILE *fp, const char *name, gint value, 
  const GEnumValue *enumValuesTable)
{
  gint     ii;

  for(ii=0; enumValuesTable[ii].value_name != NULL; ii++)
  {
    if (enumValuesTable[ii].value == value)
    {
      gap_xml_write_string_value(fp, name,enumValuesTable[ii].value_name);
      return;
    }
  }
  
  /* fallback: write unknown value as int */
  gap_xml_write_int_value(fp, name, value);
  
}  /* end p_parse_xml_value_GimpPaintmode */

