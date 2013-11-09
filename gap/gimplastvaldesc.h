/*  gimplastvaldesc.h
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains:
 *   Headers and types to register a plugin's LAST_VALUES buffer description
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
 * along with this program; if not, write to the Free Software
 * <http://www.gnu.org/licenses/>.
 */

/* revision history:
 *          2003/09/20   hof: added datatype support for guint guint32.
 *          2002/04/07   hof: created.
 */

#ifndef __GIMPLASTVALDESC_H__
#define __GIMPLASTVALDESC_H__

#ifndef __GIMP_H__
#include "libgimp/gimp.h"
#endif

typedef enum
{
  GIMP_LASTVAL_NONE,           /*   keep GIMP_LASTVAL_NONE always the first entry  */
  GIMP_LASTVAL_ARRAY,
  GIMP_LASTVAL_STRUCT_BEGIN,
  GIMP_LASTVAL_STRUCT_END,

  GIMP_LASTVAL_LONG,
  GIMP_LASTVAL_SHORT,
  GIMP_LASTVAL_INT,
  GIMP_LASTVAL_GINT,
  GIMP_LASTVAL_GINT32,
  GIMP_LASTVAL_CHAR,
  GIMP_LASTVAL_GCHAR,
  GIMP_LASTVAL_GUCHAR,
  GIMP_LASTVAL_GDOUBLE,
  GIMP_LASTVAL_GFLOAT,
  GIMP_LASTVAL_FLOAT,
  GIMP_LASTVAL_DOUBLE,
  GIMP_LASTVAL_DRAWABLE,
  GIMP_LASTVAL_GINTDRAWABLE,
  GIMP_LASTVAL_GBOOLEAN,
  GIMP_LASTVAL_ENUM,         /* not able to iterate */
  GIMP_LASTVAL_GUINT,
  GIMP_LASTVAL_GUINT32,
  GIMP_LASTVAL_END,          /* keep GIMP_LASTVAL_END always the last entry  */
} GimpLastvalType;

/* Helper MACROS for static initialisation of GimpLastvalDef  Element(s) */
#define GIMP_LASTVALDEF_NONE(flag,val,name)           { GIMP_LASTVAL_NONE,           &val, sizeof(val),         flag, name }
#define GIMP_LASTVALDEF_ARRAY(flag,elem,name)         { GIMP_LASTVAL_ARRAY,          0,     G_N_ELEMENTS(elem), flag, name }
#define GIMP_LASTVALDEF_STRUCT_BEGIN(flag,elem,name)  { GIMP_LASTVAL_STRUCT_BEGIN,   &elem, sizeof(elem),       flag, name }
#define GIMP_LASTVALDEF_STRUCT_END(flag,elem,name)    { GIMP_LASTVAL_STRUCT_END,     &elem, sizeof(elem),       flag, name }

#define GIMP_LASTVALDEF_LONG(flag,elem,name)          { GIMP_LASTVAL_LONG,           &elem, sizeof(elem),       flag, name }
#define GIMP_LASTVALDEF_SHORT(flag,elem,name)         { GIMP_LASTVAL_SHORT,          &elem, sizeof(elem),       flag, name }
#define GIMP_LASTVALDEF_INT(flag,elem,name)           { GIMP_LASTVAL_INT,            &elem, sizeof(elem),       flag, name }
#define GIMP_LASTVALDEF_GINT(flag,elem,name)          { GIMP_LASTVAL_GINT,           &elem, sizeof(elem),       flag, name }
#define GIMP_LASTVALDEF_GINT32(flag,elem,name)        { GIMP_LASTVAL_GINT32,         &elem, sizeof(elem),       flag, name }
#define GIMP_LASTVALDEF_CHAR(flag,elem,name)          { GIMP_LASTVAL_CHAR,           &elem, sizeof(elem),       flag, name }
#define GIMP_LASTVALDEF_GCHAR(flag,elem,name)         { GIMP_LASTVAL_GCHAR,          &elem, sizeof(elem),       flag, name }
#define GIMP_LASTVALDEF_GUCHAR(flag,elem,name)        { GIMP_LASTVAL_GUCHAR,         &elem, sizeof(elem),       flag, name }
#define GIMP_LASTVALDEF_GDOUBLE(flag,elem,name)       { GIMP_LASTVAL_GDOUBLE,        &elem, sizeof(elem),       flag, name }
#define GIMP_LASTVALDEF_GFLOAT(flag,elem,name)        { GIMP_LASTVAL_GFLOAT,         &elem, sizeof(elem),       flag, name }
#define GIMP_LASTVALDEF_FLOAT(flag,elem,name)         { GIMP_LASTVAL_FLOAT,          &elem, sizeof(elem),       flag, name }
#define GIMP_LASTVALDEF_DOUBLE(flag,elem,name)        { GIMP_LASTVAL_DOUBLE,         &elem, sizeof(elem),       flag, name }
#define GIMP_LASTVALDEF_DRAWABLE(flag,elem,name)      { GIMP_LASTVAL_DRAWABLE,       &elem, sizeof(elem),       flag, name }
#define GIMP_LASTVALDEF_GINTDRAWABLE(flag,elem,name)  { GIMP_LASTVAL_GINTDRAWABLE,   &elem, sizeof(elem),       flag, name }
#define GIMP_LASTVALDEF_GBOOLEAN(flag,elem,name)      { GIMP_LASTVAL_GBOOLEAN,       &elem, sizeof(elem),       flag, name }
#define GIMP_LASTVALDEF_ENUM(flag,elem,name)          { GIMP_LASTVAL_ENUM,           &elem, sizeof(elem),       flag, name }
#define GIMP_LASTVALDEF_GUINT(flag,elem,name)         { GIMP_LASTVAL_GUINT,          &elem, sizeof(elem),       flag, name }
#define GIMP_LASTVALDEF_GUINT32(flag,elem,name)       { GIMP_LASTVAL_GUINT32,        &elem, sizeof(elem),       flag, name }

/* More Helper MACROS for some often used Structured Types */
#define GIMP_LASTVALDEF_GIMPRGB(flag,elem,name) \
  { GIMP_LASTVAL_STRUCT_BEGIN,   &elem,     sizeof(elem),       flag, name }, \
  { GIMP_LASTVAL_GDOUBLE,        &elem.r,   sizeof(elem.r),     flag, "r" },  \
  { GIMP_LASTVAL_GDOUBLE,        &elem.g,   sizeof(elem.g),     flag, "g" },  \
  { GIMP_LASTVAL_GDOUBLE,        &elem.b,   sizeof(elem.b),     flag, "b" },  \
  { GIMP_LASTVAL_GDOUBLE,        &elem.a,   sizeof(elem.a),     flag, "a" },  \
  { GIMP_LASTVAL_STRUCT_END,     &elem, sizeof(elem),           flag, name }

#define GIMP_LASTVALDEF_GIMPHSV(flag,elem,name) \
  { GIMP_LASTVAL_STRUCT_BEGIN,   &elem,     sizeof(elem),       flag, name }, \
  { GIMP_LASTVAL_GDOUBLE,        &elem.h,   sizeof(elem.h),     flag, "h" },  \
  { GIMP_LASTVAL_GDOUBLE,        &elem.s,   sizeof(elem.s),     flag, "s" },  \
  { GIMP_LASTVAL_GDOUBLE,        &elem.v,   sizeof(elem.v),     flag, "v" },  \
  { GIMP_LASTVAL_GDOUBLE,        &elem.a,   sizeof(elem.a),     flag, "a" },  \
  { GIMP_LASTVAL_STRUCT_END,     &elem, sizeof(elem),           flag, name }

#define GIMP_LASTVALDEF_GIMPVECTOR2(flag,elem,name) \
  { GIMP_LASTVAL_STRUCT_BEGIN,   &elem,     sizeof(elem),       flag, name }, \
  { GIMP_LASTVAL_GDOUBLE,        &elem.x,   sizeof(elem.x),     flag, "x" },  \
  { GIMP_LASTVAL_GDOUBLE,        &elem.y,   sizeof(elem.y),     flag, "y" },  \
  { GIMP_LASTVAL_STRUCT_END,     &elem, sizeof(elem),           flag, name }

#define GIMP_LASTVALDEF_GIMPVECTOR3(flag,elem,name) \
  { GIMP_LASTVAL_STRUCT_BEGIN,   &elem,     sizeof(elem),       flag, name }, \
  { GIMP_LASTVAL_GDOUBLE,        &elem.x,   sizeof(elem.x),     flag, "x" },  \
  { GIMP_LASTVAL_GDOUBLE,        &elem.y,   sizeof(elem.y),     flag, "y" },  \
  { GIMP_LASTVAL_GDOUBLE,        &elem.z,   sizeof(elem.z),     flag, "z" },  \
  { GIMP_LASTVAL_STRUCT_END,     &elem, sizeof(elem),           flag, name }



/* typenames used in the lastval_description file */
#define GIMP_DDESC_NONE              "NONE"           
#define GIMP_DDESC_ARRAY             "ARRAY"
#define GIMP_DDESC_STRUCT_BEGIN      "STRUCT_BEGIN"
#define GIMP_DDESC_STRUCT_END        "STRUCT_END"

#define GIMP_DDESC_LONG              "long"
#define GIMP_DDESC_SHORT             "short"
#define GIMP_DDESC_INT               "int"
#define GIMP_DDESC_GINT              "gint"
#define GIMP_DDESC_GINT32            "gint32"
#define GIMP_DDESC_CHAR              "char"
#define GIMP_DDESC_GCHAR             "gchar"
#define GIMP_DDESC_GUCHAR            "guchar"
#define GIMP_DDESC_GDOUBLE           "gdouble"
#define GIMP_DDESC_GFLOAT            "gfloat"
#define GIMP_DDESC_FLOAT             "float"
#define GIMP_DDESC_DOUBLE            "double"
#define GIMP_DDESC_DRAWABLE          "drawable"
#define GIMP_DDESC_GINTDRAWABLE      "gintdrawable"
#define GIMP_DDESC_GBOOLEAN          "gboolean"
#define GIMP_DDESC_ENUM              "ENUM"
#define GIMP_DDESC_GUINT             "guint"
#define GIMP_DDESC_GUINT32           "guint32"
#define GIMP_DDESC_END               "END"


#define GIMP_PLUGIN_GAP_COMMON_ITER  "plug-in-gap-COMMON-ITERATOR"

#define GIMP_ITER_TRUE  1
#define GIMP_ITER_FALSE 0

#define GAP_LASTVAL_KEY_ANIMATED_CALL_INFO "GAP_LASTVAL_KEY_ANIMATED_CALL_INFO"

typedef struct _GimpLastvalDef    GimpLastvalDef;

struct _GimpLastvalDef
{
  GimpLastvalType  type;
  void            *elem_adress;
  gint32           elem_size;
  gint32           iter_flag;  /* 0 ignore, 1 iterate */
  gchar           *elem_name;
};


typedef  struct GimpLastvalDescType
{
   GimpLastvalType lastval_type;
   gint32         offset;
   gint32         elem_size;  /* only for ARRAY and STRUCT */
   gint32         iter_flag;  /* 0 ignore, 1 iterate */
   gchar          elem_name[100];      /* parameter name */
} GimpLastvalDescType;


typedef struct
{
  gboolean  animatedCallInProgress;
  gint32    total_steps; 
  gdouble   current_step;            /* current step respecting acceleration characteristics */
} GapLastvalAnimatedCallInfo;

/* ----------------------------------
 * gimp_lastval_desc_register
 * ----------------------------------
 *   with this procedure a plug-in can register the structure of
 *   it's LAST_VALUES buffer for iteration.
 *   - the registration (call of gimp_lastval_desc_register)
 *     should be done in a plug-in's query procedure.
 *     the structure description is stored in a file in the users gimp_dir.
 *        gimp_lastval_desc_filename returns the name of that file.
 *
 *   - iteration is useful when the plug-in is called automatic
 *     in RUN_WITH_LAST_VALUES mode on many layers. (Animated filtercall)
 *     A central iterator procedure can read the registered sturcture,
 *     and slightly modify the LAST_VALUES Buffer for each step.
 *     The plug-in must not open parameter DIALOG when called
 *     in RUN_WITH_LAST_VALUES mode.
 *
 *   - describe at least all the paramters that make sense to modify (iterate)
 *     (you may use GIMP_ITER_FALSE for parameters that should stay constant.)
 *
 *   - a plug-in can write it's own private iterator. (in that case there is
 *     no need to register the structure, because the private iterator
 *     is always used rather than the common one.)
 *
 *   - plug-ins that are not able to work on a single drawable or
 *     are not able to run with LAST_VALUES can not be used in animated filtercalls
 *     (even if they register their structructure)
 *
 * ----------------------------------
 * gimp_lastval_desc_register
 * ----------------------------------
 * read all available structure description(s) from file 
 * (and set in memory for the current gimp session)
 */

gboolean  gimp_lastval_desc_register(const gchar *keyname, void  *baseadress, gint32 total_size, gint32 nlastvals, GimpLastvalDef *lastvals);
void      gimp_lastval_desc_update(void);
gchar *   gimp_lastval_desc_filename(void);
gchar *   gimp_lastval_desc_keyname(const gchar *keyname);

#endif  /* __GIMPLASTVALDESC_H__ */
