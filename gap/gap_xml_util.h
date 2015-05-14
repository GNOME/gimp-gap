/* gap_xml_util.h
 * 2011.03.09 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * functions to parse and write XML attributes for basic data types.
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
#ifndef _GAP_XML_UTIL_H
#define _GAP_XML_UTIL_H

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libgimp/gimp.h"

gboolean    gap_xml_parse_value_gdouble(const gchar *attribute_value, gdouble *valDestPtr);
gboolean    gap_xml_parse_value_gint32(const gchar *attribute_value, gint32 *valDestPtr);
gboolean    gap_xml_parse_value_gint(const gchar *attribute_value, gint *valDestPtr);
gboolean    gap_xml_parse_value_gboolean(const gchar *attribute_value, gboolean *valDestPtr);
gboolean    gap_xml_parse_value_gboolean_as_gint(const gchar *attribute_value, gint *valDestPtr);
gboolean    gap_xml_parse_EnumValue_as_gint(const gchar *attribute_value, gint *valDestPtr, const GEnumValue *enumValuesTable);
gboolean    gap_xml_parse_value_utf8_string(const gchar *attribute_value, gchar **valDestPtr);

void        gap_xml_write_gboolean_value(FILE *fp, const char *name, gboolean value);
void        gap_xml_write_gint_as_gboolean_value(FILE *fp, const char *name, gint value);
void        gap_xml_write_int_value(FILE *fp, const char *name, gint32 value);
void        gap_xml_write_gdouble_value(FILE *fp, const char *name, gdouble value, gint digits, gint precision_digits);
void        gap_xml_write_string_value(FILE *fp, const char *name, const char *value);
void        gap_xml_write_EnumValue(FILE *fp, const char *name, gint value, const GEnumValue *enumValuesTable);

#endif
 
 
