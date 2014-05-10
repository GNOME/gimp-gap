/* gap_mov_xml_par.c
 * 2014.05.07 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * Move Path : XML parameterfile load and save procedures.
 * The XML parameterfile contains full information of all parameters
 * available in the move path feature including:
 *  version
 *  frame description, moving object description
 *  tweens, trace layer, bluebox settings and controlpoints.
 *
 * Note that the old pointfile format is still supported
 * but not handled here.
 * (the old pointfile format is limited to information about the controlpoints that build the path
 * see gap_mov_exec module for load/save support of the old pointfile format)
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

/* SYTEM (UNIX) includes */
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib/gstdio.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_libgapbase.h"
#include "gap-intl.h"
#include "gap_lib.h"
#include "gap_mov_dialog.h"
#include "gap_mov_exec.h"
#include "gap_mov_render.h"
#include "gap_mov_xml_par.h"
#include "gap_accel_char.h"
#include "gap_bluebox.h"
#include "gap_xml_util.h"


/* The xml tokens (element and attribute names) */
#define GAP_MOVPATH_XML_TOKEN_ROOT                   "gimp_gap_move_path_parameters"
#define GAP_MOVPATH_XML_TOKEN_VERSION                "version"
#define GAP_MOVPATH_XML_TOKEN_FRAME_DESCRIPTION      "frame_description"
#define GAP_MOVPATH_XML_TOKEN_WIDTH                  "width"
#define GAP_MOVPATH_XML_TOKEN_HEIGHT                 "height"
#define GAP_MOVPATH_XML_TOKEN_RANGE_FROM             "range_from"
#define GAP_MOVPATH_XML_TOKEN_RANGE_TO               "range_to"
#define GAP_MOVPATH_XML_TOKEN_TOTAL_FRAMES           "total_frames"
#define GAP_MOVPATH_XML_TOKEN_TWEEN                  "tween"
#define GAP_MOVPATH_XML_TOKEN_TWEEN_STEPS            "tween_steps"
#define GAP_MOVPATH_XML_TOKEN_TWEEN_OPACITY_INITIAL  "tween_opacity_initial"
#define GAP_MOVPATH_XML_TOKEN_TWEEN_OPACITY_DESC     "tween_opacity_desc"
#define GAP_MOVPATH_XML_TOKEN_TRACE                  "trace"
#define GAP_MOVPATH_XML_TOKEN_TRACELAYER_ENABLE      "tracelayer_enable"
#define GAP_MOVPATH_XML_TOKEN_TRACE_OPACITY_INITIAL  "trace_opacity_initial"
#define GAP_MOVPATH_XML_TOKEN_TRACE_OPACITY_DESC     "trace_opacity_desc"
#define GAP_MOVPATH_XML_TOKEN_MOVING_OBJECT          "moving_object"
#define GAP_MOVPATH_XML_TOKEN_SRC_STEPMODE           "src_stepmode"
#define GAP_MOVPATH_XML_TOKEN_SRC_HANDLE             "src_handle"
#define GAP_MOVPATH_XML_TOKEN_SRC_SELMODE            "src_selmode"
#define GAP_MOVPATH_XML_TOKEN_SRC_PAINTMODE          "src_paintmode"
#define GAP_MOVPATH_XML_TOKEN_DST_LAYERSTACK         "dst_layerstack"
#define GAP_MOVPATH_XML_TOKEN_DST_GROUP_PATH         "dst_group_name_path"
#define GAP_MOVPATH_XML_TOKEN_DST_GROUP_DELIM        "dst_group_name_delimiter"

#define GAP_MOVPATH_XML_TOKEN_STEP_SPEED_FACTOR      "step_speed_factor"
#define GAP_MOVPATH_XML_TOKEN_SRC_FORCE_VISIBLE      "src_force_visible"
#define GAP_MOVPATH_XML_TOKEN_CLIP_TO_IMG            "clip_to_img"
#define GAP_MOVPATH_XML_TOKEN_SRC_APPLY_BLUEBOX      "src_apply_bluebox"
#define GAP_MOVPATH_XML_TOKEN_SRC_LAYER_ID           "src_layer_id"
#define GAP_MOVPATH_XML_TOKEN_SRC_LAYERSTACK         "src_layerstack"
#define GAP_MOVPATH_XML_TOKEN_SRC_FILENAME           "src_filename"
#define GAP_MOVPATH_XML_TOKEN_BLUEBOX_PARAMETERS     "gimp_gap_bluebox_parameters"
#define GAP_MOVPATH_XML_TOKEN_THRES_MODE             "thres_mode"
#define GAP_MOVPATH_XML_TOKEN_THRES_R                "thres_r"
#define GAP_MOVPATH_XML_TOKEN_THRES_G                "thres_g"
#define GAP_MOVPATH_XML_TOKEN_THRES_B                "thres_b"
#define GAP_MOVPATH_XML_TOKEN_THRES_H                "thres_h"
#define GAP_MOVPATH_XML_TOKEN_THRES_S                "thres_s"
#define GAP_MOVPATH_XML_TOKEN_THRES_V                "thres_v"
#define GAP_MOVPATH_XML_TOKEN_THRES                  "thres"
#define GAP_MOVPATH_XML_TOKEN_TOLERANCE              "tolerance"
#define GAP_MOVPATH_XML_TOKEN_GROW                   "grow"
#define GAP_MOVPATH_XML_TOKEN_FEATHER_EDGES          "feather_edges"
#define GAP_MOVPATH_XML_TOKEN_FEATHER_RADIUS         "feather_radius"
#define GAP_MOVPATH_XML_TOKEN_SOURCE_ALPHA           "source_alpha"
#define GAP_MOVPATH_XML_TOKEN_TARGET_ALPHA           "target_alpha"
#define GAP_MOVPATH_XML_TOKEN_KEYCOLOR_R             "keycolor_r"
#define GAP_MOVPATH_XML_TOKEN_KEYCOLOR_G             "keycolor_g"
#define GAP_MOVPATH_XML_TOKEN_KEYCOLOR_B             "keycolor_b"
#define GAP_MOVPATH_XML_TOKEN_CONTROLPOINTS          "controlpoints"
#define GAP_MOVPATH_XML_TOKEN_CURRENT_POINT          "current_point"
#define GAP_MOVPATH_XML_TOKEN_NUMBER_OF_POINTS       "number_of_points"
#define GAP_MOVPATH_XML_TOKEN_ROTATE_THRESHOLD       "rotate_threshold"
#define GAP_MOVPATH_XML_TOKEN_CONTROLPOINT           "controlpoint"
#define GAP_MOVPATH_XML_TOKEN_PX                     "px"
#define GAP_MOVPATH_XML_TOKEN_PY                     "py"
#define GAP_MOVPATH_XML_TOKEN_KEYFRAME               "keyframe"
#define GAP_MOVPATH_XML_TOKEN_KEYFRAME_ABS           "keyframe_abs"
#define GAP_MOVPATH_XML_TOKEN_SEL_FEATHER_RADIUS     "sel_feather_radius"
#define GAP_MOVPATH_XML_TOKEN_W_RESIZE               "width_resize"
#define GAP_MOVPATH_XML_TOKEN_H_RESIZE               "height_resize"
#define GAP_MOVPATH_XML_TOKEN_OPACITY                "opacity"
#define GAP_MOVPATH_XML_TOKEN_ROTATION               "rotation"
#define GAP_MOVPATH_XML_TOKEN_TTLX                   "ttlx"
#define GAP_MOVPATH_XML_TOKEN_TTLY                   "ttly"
#define GAP_MOVPATH_XML_TOKEN_TTRX                   "ttrx"
#define GAP_MOVPATH_XML_TOKEN_TTRY                   "ttry"
#define GAP_MOVPATH_XML_TOKEN_TBLX                   "tblx"
#define GAP_MOVPATH_XML_TOKEN_TBLY                   "tbly"
#define GAP_MOVPATH_XML_TOKEN_TBRX                   "tbrx"
#define GAP_MOVPATH_XML_TOKEN_TBRY                   "tbry"
#define GAP_MOVPATH_XML_TOKEN_ACC_POSITION           "acc_position"
#define GAP_MOVPATH_XML_TOKEN_ACC_OPACITY            "acc_opacity"
#define GAP_MOVPATH_XML_TOKEN_ACC_SIZE               "acc_size"
#define GAP_MOVPATH_XML_TOKEN_ACC_ROTATION           "acc_rotation"
#define GAP_MOVPATH_XML_TOKEN_ACC_PERSPECTIVE        "acc_perspective"
#define GAP_MOVPATH_XML_TOKEN_ACC_SEL_FEATHER_RADIUS "acc_sel_feather_radius"




/* user data passed to parser fuctions */
typedef struct {
  GapMovValues *pvals;
  gboolean      isScopeValid;
  gboolean      isParseOk;
  gint          errorLineNumber;
} GapMovXmlUserData;

/* Function signatures for parsing xml elements and attributes
 */
typedef void (*GapMovXmlElementParseFunctionType) (const gchar         *element_name,
    const gchar        **attribute_names,
    const gchar        **attribute_values,
    GapMovXmlUserData   *userDataPtr,
    gint                count
    );


/* functions for parsing the XML elements */

static void  p_xml_parse_element_root(const gchar         *element_name,
    const gchar        **attribute_names,
    const gchar        **attribute_values,
    GapMovXmlUserData   *userDataPtr,
    gint                 count);
static void  p_xml_parse_element_frame_description(const gchar         *element_name,
    const gchar        **attribute_names,
    const gchar        **attribute_values,
    GapMovXmlUserData   *userDataPtr,
    gint                 count);
static void  p_xml_parse_element_tween(const gchar         *element_name,
    const gchar        **attribute_names,
    const gchar        **attribute_values,
    GapMovXmlUserData   *userDataPtr,
    gint                 count);
static void  p_xml_parse_element_trace(const gchar         *element_name,
    const gchar        **attribute_names,
    const gchar        **attribute_values,
    GapMovXmlUserData   *userDataPtr,
    gint                 count);
static void  p_xml_parse_element_moving_object(const gchar         *element_name,
    const gchar        **attribute_names,
    const gchar        **attribute_values,
    GapMovXmlUserData   *userDataPtr,
    gint                 count);
static void  p_xml_parse_element_trace(const gchar         *element_name,
    const gchar        **attribute_names,
    const gchar        **attribute_values,
    GapMovXmlUserData   *userDataPtr,
    gint                 count);
static void  p_xml_parse_element_bluebox_parameters(const gchar         *element_name,
    const gchar        **attribute_names,
    const gchar        **attribute_values,
    GapMovXmlUserData   *userDataPtr,
    gint                 count);
static void  p_xml_parse_element_controlpoints(const gchar         *element_name,
    const gchar        **attribute_names,
    const gchar        **attribute_values,
    GapMovXmlUserData   *userDataPtr,
    gint                 count);
static void  p_xml_parse_element_controlpoint(const gchar         *element_name,
    const gchar        **attribute_names,
    const gchar        **attribute_values,
    GapMovXmlUserData   *userDataPtr,
    gint                 count);




typedef struct JmpTableElement
{
  gint                                 count;
  gboolean                             isRequired;
  const gchar                         *name;
  GapMovXmlElementParseFunctionType    func_ptr;
} JmpTableElement;


/* ------------------------
 * STATIC DATA
 * ------------------------
 */


extern      int gap_debug; /* ==0  ... dont print debug infos */


/* jump table describes the supported xml element names and their parsing functions */
static  JmpTableElement    jmpTableElement[] = {
  {0, TRUE,  GAP_MOVPATH_XML_TOKEN_ROOT,                    p_xml_parse_element_root }
 ,{0, TRUE,  GAP_MOVPATH_XML_TOKEN_FRAME_DESCRIPTION,       p_xml_parse_element_frame_description }
 ,{0, FALSE, GAP_MOVPATH_XML_TOKEN_TWEEN,                   p_xml_parse_element_tween }
 ,{0, FALSE, GAP_MOVPATH_XML_TOKEN_TRACE,                   p_xml_parse_element_trace }
 ,{0, TRUE,  GAP_MOVPATH_XML_TOKEN_MOVING_OBJECT,           p_xml_parse_element_moving_object }
 ,{0, FALSE, GAP_MOVPATH_XML_TOKEN_BLUEBOX_PARAMETERS,      p_xml_parse_element_bluebox_parameters }
 ,{0, TRUE,  GAP_MOVPATH_XML_TOKEN_CONTROLPOINTS,           p_xml_parse_element_controlpoints }
 ,{0, TRUE,  GAP_MOVPATH_XML_TOKEN_CONTROLPOINT,            p_xml_parse_element_controlpoint }
 ,{0, FALSE, NULL,                                          NULL }
};


static const GEnumValue valuesGapMovHandle[] =
{
  { GAP_HANDLE_LEFT_TOP,         "GAP_HANDLE_LEFT_TOP", NULL },
  { GAP_HANDLE_LEFT_BOT,         "GAP_HANDLE_LEFT_BOT", NULL },
  { GAP_HANDLE_RIGHT_TOP,        "GAP_HANDLE_RIGHT_TOP", NULL },
  { GAP_HANDLE_RIGHT_BOT,        "GAP_HANDLE_RIGHT_BOT", NULL },
  { GAP_HANDLE_CENTER,           "GAP_HANDLE_CENTER", NULL },
  { 0,                           NULL, NULL }
};

static const GEnumValue valuesGapMovStepMode[] =
{
  { GAP_STEP_LOOP,              "GAP_STEP_LOOP", NULL },
  { GAP_STEP_LOOP_REV,          "GAP_STEP_LOOP_REV", NULL },
  { GAP_STEP_ONCE,              "GAP_STEP_ONCE", NULL },
  { GAP_STEP_ONCE_REV,          "GAP_STEP_ONCE_REV", NULL },
  { GAP_STEP_PING_PONG,         "GAP_STEP_PING_PONG", NULL },
  { GAP_STEP_NONE,              "GAP_STEP_NONE", NULL },
  { GAP_STEP_FRAME_LOOP,        "GAP_STEP_FRAME_LOOP", NULL },
  { GAP_STEP_FRAME_LOOP_REV,    "GAP_STEP_FRAME_LOOP_REV", NULL },
  { GAP_STEP_FRAME_ONCE,        "GAP_STEP_FRAME_ONCE", NULL },
  { GAP_STEP_FRAME_ONCE_REV,    "GAP_STEP_FRAME_ONCE_REV", NULL },
  { GAP_STEP_FRAME_PING_PONG,   "GAP_STEP_FRAME_PING_PONG", NULL },
  { GAP_STEP_FRAME_NONE,        "GAP_STEP_FRAME_NONE", NULL },
  { 0,                          NULL, NULL }
};

static const GEnumValue valuesGapMovSelMode[] =
{
  { GAP_MOV_SEL_IGNORE,         "GAP_MOV_SEL_IGNORE", NULL },
  { GAP_MOV_SEL_INITIAL,        "GAP_MOV_SEL_INITIAL", NULL },
  { GAP_MOV_SEL_FRAME_SPECIFIC, "GAP_MOV_SEL_FRAME_SPECIFIC", NULL },
  { 0,                          NULL, NULL }
};

static const GEnumValue valuesGimpPaintmode[] =
{
  { GIMP_NORMAL_MODE,           "GIMP_NORMAL_MODE", "normal-mode" },
  { GIMP_DISSOLVE_MODE,         "GIMP_DISSOLVE_MODE", "dissolve-mode" },
  { GIMP_BEHIND_MODE,           "GIMP_BEHIND_MODE", "behind-mode" },
  { GIMP_MULTIPLY_MODE,         "GIMP_MULTIPLY_MODE", "multiply-mode" },
  { GIMP_SCREEN_MODE,           "GIMP_SCREEN_MODE", "screen-mode" },
  { GIMP_OVERLAY_MODE,          "GIMP_OVERLAY_MODE", "overlay-mode" },
  { GIMP_DIFFERENCE_MODE,       "GIMP_DIFFERENCE_MODE", "difference-mode" },
  { GIMP_ADDITION_MODE,         "GIMP_ADDITION_MODE", "addition-mode" },
  { GIMP_SUBTRACT_MODE,         "GIMP_SUBTRACT_MODE", "subtract-mode" },
  { GIMP_DARKEN_ONLY_MODE,      "GIMP_DARKEN_ONLY_MODE", "darken-only-mode" },
  { GIMP_LIGHTEN_ONLY_MODE,     "GIMP_LIGHTEN_ONLY_MODE", "lighten-only-mode" },
  { GIMP_HUE_MODE,              "GIMP_HUE_MODE", "hue-mode" },
  { GIMP_SATURATION_MODE,       "GIMP_SATURATION_MODE", "saturation-mode" },
  { GIMP_COLOR_MODE,            "GIMP_COLOR_MODE", "color-mode" },
  { GIMP_VALUE_MODE,            "GIMP_VALUE_MODE", "value-mode" },
  { GIMP_DIVIDE_MODE,           "GIMP_DIVIDE_MODE", "divide-mode" },
  { GIMP_DODGE_MODE,            "GIMP_DODGE_MODE", "dodge-mode" },
  { GIMP_BURN_MODE,             "GIMP_BURN_MODE", "burn-mode" },
  { GIMP_HARDLIGHT_MODE,        "GIMP_HARDLIGHT_MODE", "hardlight-mode" },
  { GIMP_SOFTLIGHT_MODE,        "GIMP_SOFTLIGHT_MODE", "softlight-mode" },
  { GIMP_GRAIN_EXTRACT_MODE,    "GIMP_GRAIN_EXTRACT_MODE", "grain-extract-mode" },
  { GIMP_GRAIN_MERGE_MODE,      "GIMP_GRAIN_MERGE_MODE", "grain-merge-mode" },
  { GIMP_COLOR_ERASE_MODE,      "GIMP_COLOR_ERASE_MODE", "color-erase-mode" },
  { GAP_MOV_KEEP_SRC_PAINTMODE, "GAP_MOV_KEEP_SRC_PAINTMODE", "keep-paintmode" },
  { 0,                          NULL, NULL }
};



static const GEnumValue valuesGapBlueboxThresMode[] =
{
  { GAP_BLUBOX_THRES_RGB,          "GAP_BLUBOX_THRES_RGB", NULL },
  { GAP_BLUBOX_THRES_HSV,          "GAP_BLUBOX_THRES_HSV", NULL },
  { GAP_BLUBOX_THRES_VAL,          "GAP_BLUBOX_THRES_VAL", NULL },
  { GAP_BLUBOX_THRES_ALL,          "GAP_BLUBOX_THRES_ALL", NULL },
  { 0,                             NULL, NULL }
};



/*
 * XML PARSER procedures
 */



/* --------------------------------------
 * p_xml_parse_value_GapMovHandle
 * --------------------------------------
 */
static gboolean
p_xml_parse_value_GapMovHandle(const gchar *attribute_value, GapMovHandle *valDestPtr)
{
  gboolean isOk;
  gint     value;

  isOk = gap_xml_parse_EnumValue_as_gint(attribute_value, &value, &valuesGapMovHandle[0]);
  if(isOk)
  {
    *valDestPtr = value;
  }
  return (isOk);

}  /* end p_xml_parse_value_GapMovHandle */



/* --------------------------------------
 * p_xml_parse_value_GapMovStepMode
 * --------------------------------------
 */
static gboolean
p_xml_parse_value_GapMovStepMode(const gchar *attribute_value, GapMovStepMode *valDestPtr)
{
  gboolean isOk;
  gint     value;

  isOk = gap_xml_parse_EnumValue_as_gint(attribute_value, &value, &valuesGapMovStepMode[0]);
  if(isOk)
  {
    *valDestPtr = value;
  }
  return (isOk);

}  /* end p_xml_parse_value_GapMovStepMode */


/* --------------------------------------
 * p_xml_parse_value_GapMovSelMode
 * --------------------------------------
 */
static gboolean
p_xml_parse_value_GapMovSelMode(const gchar *attribute_value, GapMovSelMode *valDestPtr)
{
  gboolean isOk;
  gint     value;

  isOk = gap_xml_parse_EnumValue_as_gint(attribute_value, &value, &valuesGapMovSelMode[0]);
  if(isOk)
  {
    *valDestPtr = value;
  }
  return (isOk);

}  /* end p_xml_parse_value_GapMovSelMode */


/* ---------------------------------------
 * p_xml_parse_value_GimpPaintmode_as_gint
 * ---------------------------------------
 */
static gboolean
p_xml_parse_value_GimpPaintmode_as_gint(const gchar *attribute_value, gint *valDestPtr)
{
  gboolean isOk;
  gint     value;

  isOk = gap_xml_parse_EnumValue_as_gint(attribute_value, &value, &valuesGimpPaintmode[0]);
  if(isOk)
  {
    *valDestPtr = value;
  }
  return (isOk);

}  /* end gap_xml_parse_value_GimpPaintmode */


/* ----------------------------------------
 * p_xml_parse_value_GapBlueboxThresMode
 * ----------------------------------------
 */
static gboolean
p_xml_parse_value_GapBlueboxThresMode(const gchar *attribute_value, GapBlueboxThresMode *valDestPtr)
{
  gboolean isOk;
  gint     value;

  isOk = gap_xml_parse_EnumValue_as_gint(attribute_value, &value, &valuesGapBlueboxThresMode[0]);
  if(isOk)
  {
    *valDestPtr = value;
  }
  return (isOk);

}  /* end p_xml_parse_value_GapBlueboxThresMode */




/* --------------------------------------
 * p_xml_parse_element_root
 * --------------------------------------
 */
static void
p_xml_parse_element_root(const gchar         *element_name,
    const gchar        **attribute_names,
    const gchar        **attribute_values,
    GapMovXmlUserData   *userDataPtr,
    gint                 count)
{
  const gchar **name_cursor = attribute_names;
  const gchar **value_cursor = attribute_values;

  if(count > 0)
  {
    userDataPtr->isParseOk = FALSE;
  }

  while ((*name_cursor) && (userDataPtr->isParseOk))
  {
    if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_VERSION) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gint32(*value_cursor, &userDataPtr->pvals->version);
    }
    name_cursor++;
    value_cursor++;
  }
}   /* end  p_xml_parse_element_root */




/* --------------------------------------
 * p_xml_parse_element_frame_description
 * --------------------------------------
 */
static void
p_xml_parse_element_frame_description(const gchar         *element_name,
    const gchar        **attribute_names,
    const gchar        **attribute_values,
    GapMovXmlUserData   *userDataPtr,
    gint                 count)
{
  const gchar **name_cursor = attribute_names;
  const gchar **value_cursor = attribute_values;

  if(count > 0)
  {
    userDataPtr->isParseOk = FALSE;
  }

  while ((*name_cursor) && (userDataPtr->isParseOk))
  {
    if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_WIDTH) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gint32(*value_cursor, &userDataPtr->pvals->recordedFrameWidth);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_HEIGHT) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gint32(*value_cursor, &userDataPtr->pvals->recordedFrameHeight);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_RANGE_FROM) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gint(*value_cursor, &userDataPtr->pvals->dst_range_start);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_RANGE_TO) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gint(*value_cursor, &userDataPtr->pvals->dst_range_end);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_RANGE_TO) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gint32(*value_cursor, &userDataPtr->pvals->total_frames);
    }

    name_cursor++;
    value_cursor++;
  }
}   /* end  p_xml_parse_element_frame_description */


/* --------------------------------------
 * p_xml_parse_element_tween
 * --------------------------------------
 */
static void
p_xml_parse_element_tween(const gchar         *element_name,
    const gchar        **attribute_names,
    const gchar        **attribute_values,
    GapMovXmlUserData   *userDataPtr,
    gint                 count)
{
  const gchar **name_cursor = attribute_names;
  const gchar **value_cursor = attribute_values;

  if(count > 0)
  {
    userDataPtr->isParseOk = FALSE;
  }

  while ((*name_cursor) && (userDataPtr->isParseOk))
  {
    if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_TWEEN_STEPS) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gint(*value_cursor, &userDataPtr->pvals->tween_steps);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_TWEEN_OPACITY_INITIAL) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &userDataPtr->pvals->tween_opacity_initial);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_TWEEN_OPACITY_DESC) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &userDataPtr->pvals->tween_opacity_desc);
    }
    name_cursor++;
    value_cursor++;
  }
}   /* end  p_xml_parse_element_tween */




/* --------------------------------------
 * p_xml_parse_element_trace
 * --------------------------------------
 */
static void
p_xml_parse_element_trace(const gchar         *element_name,
    const gchar        **attribute_names,
    const gchar        **attribute_values,
    GapMovXmlUserData   *userDataPtr,
    gint                 count)
{
  const gchar **name_cursor = attribute_names;
  const gchar **value_cursor = attribute_values;

  if(count > 0)
  {
    userDataPtr->isParseOk = FALSE;
  }

  while ((*name_cursor) && (userDataPtr->isParseOk))
  {
    if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_TRACELAYER_ENABLE) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gboolean_as_gint(*value_cursor, &userDataPtr->pvals->tracelayer_enable);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_TRACE_OPACITY_INITIAL) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &userDataPtr->pvals->trace_opacity_initial);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_TRACE_OPACITY_DESC) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &userDataPtr->pvals->trace_opacity_desc);
    }
    name_cursor++;
    value_cursor++;
  }
}   /* end  p_xml_parse_element_trace */




/* --------------------------------------
 * p_xml_parse_element_moving_object
 * --------------------------------------
 */
static void
p_xml_parse_element_moving_object(const gchar         *element_name,
    const gchar        **attribute_names,
    const gchar        **attribute_values,
    GapMovXmlUserData   *userDataPtr,
    gint                 count)
{
  const gchar **name_cursor = attribute_names;
  const gchar **value_cursor = attribute_values;

  if(count > 0)
  {
    userDataPtr->isParseOk = FALSE;
  }

  while ((*name_cursor) && (userDataPtr->isParseOk))
  {
    if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_SRC_STEPMODE) == 0)
    {
      userDataPtr->isParseOk = p_xml_parse_value_GapMovStepMode(*value_cursor, &userDataPtr->pvals->src_stepmode);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_WIDTH) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gint32(*value_cursor, &userDataPtr->pvals->recordedObjWidth);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_HEIGHT) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gint32(*value_cursor, &userDataPtr->pvals->recordedObjHeight);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_SRC_HANDLE) == 0)
    {
      userDataPtr->isParseOk = p_xml_parse_value_GapMovHandle(*value_cursor, &userDataPtr->pvals->src_handle);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_SRC_SELMODE) == 0)
    {
      userDataPtr->isParseOk = p_xml_parse_value_GapMovSelMode(*value_cursor, &userDataPtr->pvals->src_selmode);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_SRC_PAINTMODE) == 0)
    {
      userDataPtr->isParseOk = p_xml_parse_value_GimpPaintmode_as_gint(*value_cursor, &userDataPtr->pvals->src_paintmode);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_DST_LAYERSTACK) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gint(*value_cursor, &userDataPtr->pvals->dst_layerstack);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_DST_GROUP_PATH) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_utf8_string(*value_cursor, &userDataPtr->pvals->dst_group_name_path_string);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_DST_GROUP_DELIM) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_utf8_string(*value_cursor, &userDataPtr->pvals->dst_group_name_delimiter);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_STEP_SPEED_FACTOR) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &userDataPtr->pvals->step_speed_factor);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_SRC_FORCE_VISIBLE) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gboolean_as_gint(*value_cursor, &userDataPtr->pvals->src_force_visible);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_CLIP_TO_IMG) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gboolean_as_gint(*value_cursor, &userDataPtr->pvals->clip_to_img);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_SRC_APPLY_BLUEBOX) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gboolean_as_gint(*value_cursor, &userDataPtr->pvals->src_apply_bluebox);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_SRC_LAYERSTACK) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gint32(*value_cursor, &userDataPtr->pvals->src_layerstack);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_SRC_FILENAME) == 0)
    {
      if(userDataPtr->pvals->src_filename != NULL)
      {
        g_free(userDataPtr->pvals->src_filename);
        userDataPtr->pvals->src_filename = NULL;
      }
      if(*value_cursor != NULL)
      {
        userDataPtr->pvals->src_filename = g_strdup(*value_cursor);
      }
    }

    name_cursor++;
    value_cursor++;
  }
}   /* end  p_xml_parse_element_moving_object */


/* --------------------------------------
 * p_xml_parse_element_bluebox_parameters
 * --------------------------------------
 */
static void
p_xml_parse_element_bluebox_parameters(const gchar         *element_name,
    const gchar        **attribute_names,
    const gchar        **attribute_values,
    GapMovXmlUserData   *userDataPtr,
    gint                 count)
{
  const gchar **name_cursor = attribute_names;
  const gchar **value_cursor = attribute_values;

  GapBlueboxGlobalParams *bbp;

  if(count > 0)
  {
    userDataPtr->isParseOk = FALSE;
  }

  if(userDataPtr->pvals->bbp == NULL)
  {
    gint32 layer_id;

    layer_id = -1;
    userDataPtr->pvals->bbp = gap_bluebox_bbp_new(layer_id);
    gap_bluebox_init_default_vals(userDataPtr->pvals->bbp);
  }
  bbp = userDataPtr->pvals->bbp;


  while ((*name_cursor) && (userDataPtr->isParseOk))
  {
    if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_THRES_MODE) == 0)
    {
      userDataPtr->isParseOk = p_xml_parse_value_GapBlueboxThresMode(*value_cursor, &bbp->vals.thres_mode);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_THRES_R) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &bbp->vals.thres_r);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_THRES_G) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &bbp->vals.thres_g);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_THRES_B) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &bbp->vals.thres_b);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_THRES_H) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &bbp->vals.thres_h);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_THRES_S) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &bbp->vals.thres_s);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_THRES_V) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &bbp->vals.thres_v);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_THRES) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &bbp->vals.thres);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_TOLERANCE) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &bbp->vals.tolerance);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_GROW) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &bbp->vals.grow);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_FEATHER_EDGES) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gboolean_as_gint(*value_cursor, &bbp->vals.feather_edges);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_FEATHER_RADIUS) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &bbp->vals.feather_radius);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_SOURCE_ALPHA) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &bbp->vals.source_alpha);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_TARGET_ALPHA) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &bbp->vals.target_alpha);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_KEYCOLOR_R) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &bbp->vals.keycolor.r);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_KEYCOLOR_G) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &bbp->vals.keycolor.g);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_KEYCOLOR_B) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &bbp->vals.keycolor.b);
    }

    name_cursor++;
    value_cursor++;
  }
}   /* end  p_xml_parse_element_bluebox_parameters */


/* --------------------------------------
 * p_xml_parse_element_controlpoints
 * --------------------------------------
 */
static void
p_xml_parse_element_controlpoints(const gchar         *element_name,
    const gchar        **attribute_names,
    const gchar        **attribute_values,
    GapMovXmlUserData   *userDataPtr,
    gint                 count)
{
  const gchar **name_cursor = attribute_names;
  const gchar **value_cursor = attribute_values;

  if(count > 0)
  {
    userDataPtr->isParseOk = FALSE;
  }

  while ((*name_cursor) && (userDataPtr->isParseOk))
  {
    if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_CURRENT_POINT) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gint(*value_cursor, &userDataPtr->pvals->point_idx);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_ROTATE_THRESHOLD) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &userDataPtr->pvals->rotate_threshold);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_NUMBER_OF_POINTS) == 0)
    {
      gint numberOfPoints;
      userDataPtr->isParseOk = gap_xml_parse_value_gint(*value_cursor, &numberOfPoints);

      if(userDataPtr->isParseOk)
      {
        if(numberOfPoints > 0)
        {
          if (numberOfPoints > userDataPtr->pvals->point_table_size)
          {
            gap_mov_exec_dim_point_table(userDataPtr->pvals, numberOfPoints + 2);
          }
          userDataPtr->pvals->point_idx_max = numberOfPoints -1;
        }
        else
        {
          userDataPtr->isParseOk = FALSE;
        }
      }

    }

    name_cursor++;
    value_cursor++;
  }
}   /* end  p_xml_parse_element_controlpoints */



/* ----------------------------------------
 * p_set_load_defaults_for_one_controlpoint
 * ----------------------------------------
 */
static void
p_set_load_defaults_for_one_controlpoint(GapMovValues *pvals, gint idx)
{
  if((idx >= 0) && (idx < pvals->point_table_size))
  {
    pvals->point[idx].p_x  = 0;
    pvals->point[idx].p_y  = 0;
    pvals->point[idx].opacity  = 100.0; /* 100 percent (no transparecy) */
    pvals->point[idx].w_resize = 100.0; /* 100%  no resizize (1:1) */
    pvals->point[idx].h_resize = 100.0; /* 100%  no resizize (1:1) */
    pvals->point[idx].rotation = 0.0;   /* no rotation (0 degree) */
    /* 1.0 for all perspective transform factors (== no perspective transformation) */
    pvals->point[idx].ttlx      = 1.0;
    pvals->point[idx].ttly      = 1.0;
    pvals->point[idx].ttrx      = 1.0;
    pvals->point[idx].ttry      = 1.0;
    pvals->point[idx].tblx      = 1.0;
    pvals->point[idx].tbly      = 1.0;
    pvals->point[idx].tbrx      = 1.0;
    pvals->point[idx].tbry      = 1.0;
    pvals->point[idx].sel_feather_radius = 0.0;
    pvals->point[idx].keyframe = 0;   /* 0: controlpoint is not fixed to keyframe */
    pvals->point[idx].keyframe_abs = 0;   /* 0: controlpoint is not fixed to keyframe */

    pvals->point[idx].accPosition = 0;           /* 0: linear (e.g NO acceleration) is default */
    pvals->point[idx].accOpacity = 0;            /* 0: linear (e.g NO acceleration) is default */
    pvals->point[idx].accSize = 0;               /* 0: linear (e.g NO acceleration) is default */
    pvals->point[idx].accRotation = 0;           /* 0: linear (e.g NO acceleration) is default */
    pvals->point[idx].accPerspective = 0;        /* 0: linear (e.g NO acceleration) is default */
    pvals->point[idx].accSelFeatherRadius = 0;   /* 0: linear (e.g NO acceleration) is default */

  }

}  /* end p_set_load_defaults_for_one_controlpoint  */



/* --------------------------------------
 * p_xml_parse_element_controlpoint
 * --------------------------------------
 */
static void
p_xml_parse_element_controlpoint(const gchar         *element_name,
    const gchar        **attribute_names,
    const gchar        **attribute_values,
    GapMovXmlUserData   *userDataPtr,
    gint                 count)
{
  const gchar **name_cursor = attribute_names;
  const gchar **value_cursor = attribute_values;

  if(count >= userDataPtr->pvals->point_table_size)
  {
    gap_mov_exec_dim_point_table(userDataPtr->pvals, count + GAP_MOV_POINT_EXPAND_SIZE);
  }

  p_set_load_defaults_for_one_controlpoint(userDataPtr->pvals, count);

  while ((*name_cursor) && (userDataPtr->isParseOk))
  {
    if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_PX) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gint(*value_cursor, &userDataPtr->pvals->point[count].p_x);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_PY) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gint(*value_cursor, &userDataPtr->pvals->point[count].p_y);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_KEYFRAME) == 0)
    {
      gint keyframe;

      userDataPtr->isParseOk = gap_xml_parse_value_gint(*value_cursor, &keyframe);
      userDataPtr->pvals->point[count].keyframe = keyframe;
      userDataPtr->pvals->point[count].keyframe_abs = gap_mov_exec_conv_keyframe_to_abs(keyframe, userDataPtr->pvals);

    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_KEYFRAME_ABS) == 0)
    {
      gint keyframe_abs;

      userDataPtr->isParseOk = gap_xml_parse_value_gint(*value_cursor, &keyframe_abs);
      userDataPtr->pvals->point[count].keyframe_abs = keyframe_abs;
      userDataPtr->pvals->point[count].keyframe = gap_mov_exec_conv_keyframe_to_rel(keyframe_abs, userDataPtr->pvals);

    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_SEL_FEATHER_RADIUS) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &userDataPtr->pvals->point[count].sel_feather_radius);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_W_RESIZE) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &userDataPtr->pvals->point[count].w_resize);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_H_RESIZE) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &userDataPtr->pvals->point[count].h_resize);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_OPACITY) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &userDataPtr->pvals->point[count].opacity);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_ROTATION) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &userDataPtr->pvals->point[count].rotation);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_TTLX) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &userDataPtr->pvals->point[count].ttlx);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_TTLY) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &userDataPtr->pvals->point[count].ttly);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_TTRX) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &userDataPtr->pvals->point[count].ttrx);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_TTRY) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &userDataPtr->pvals->point[count].ttry);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_TBLX) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &userDataPtr->pvals->point[count].tblx);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_TBLY) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &userDataPtr->pvals->point[count].tbly);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_TBRX) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &userDataPtr->pvals->point[count].tbrx);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_TBRY) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gdouble(*value_cursor, &userDataPtr->pvals->point[count].tbry);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_ACC_POSITION) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gint(*value_cursor, &userDataPtr->pvals->point[count].accPosition);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_ACC_OPACITY) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gint(*value_cursor, &userDataPtr->pvals->point[count].accOpacity);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_ACC_SIZE) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gint(*value_cursor, &userDataPtr->pvals->point[count].accSize);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_ACC_ROTATION) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gint(*value_cursor, &userDataPtr->pvals->point[count].accRotation);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_ACC_PERSPECTIVE) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gint(*value_cursor, &userDataPtr->pvals->point[count].accPerspective);
    }
    else if (strcmp (*name_cursor, GAP_MOVPATH_XML_TOKEN_ACC_SEL_FEATHER_RADIUS) == 0)
    {
      userDataPtr->isParseOk = gap_xml_parse_value_gint(*value_cursor, &userDataPtr->pvals->point[count].accSelFeatherRadius);
    }

    name_cursor++;
    value_cursor++;
  }
}   /* end  p_xml_parse_element_controlpoint */


/* --------------------------------------
 * p_start_xml_element
 * --------------------------------------
 * handler function to be called by the GMarkupParser
 * this handler is called each time the parser recognizes
 * the start event of an xml element.
 */
static void
p_start_xml_element (GMarkupParseContext *context,
    const gchar         *element_name,
    const gchar        **attribute_names,
    const gchar        **attribute_values,
    gpointer           user_data,
    GError             **error)
{
  gint jj;
  GapMovXmlUserData   *userDataPtr;
  
  userDataPtr = (GapMovXmlUserData *)user_data;

  if(gap_debug)
  {
    printf("p_start_xml_element: %s\n", element_name);
  }

  if(userDataPtr == NULL)
  {
    return;
  }

  for(jj=0; jmpTableElement[jj].name != NULL; jj++)
  {
    if(strcmp(jmpTableElement[jj].name, element_name) == 0)
    {
      if ((jj==0) && (jmpTableElement[jj].count == 0))
      {
        userDataPtr->isScopeValid = TRUE;
      }
      if(!userDataPtr->isScopeValid)
      {
        /* stop parsing when outsided of known namespace
         * (and stop on duplicate root element too)
         */
        return;
      }

      /* call token specific parse function */
      jmpTableElement[jj].func_ptr(element_name
                                 , attribute_names
                                 , attribute_values
                                 , userDataPtr
                                 , jmpTableElement[jj].count
                                 );
    }
    if(!userDataPtr->isParseOk)
    {
      return;
    }
  }

}  /* end p_start_xml_element */



/* --------------------------------------
 * p_end_xml_element
 * --------------------------------------
 * handler function to be called by the GMarkupParser
 * this handler is called each time the parser recognizes
 * the end event of an xml element
 */
static void
p_end_xml_element (GMarkupParseContext *context,
    const gchar         *element_name,
    gpointer             user_data,
    GError             **error)
{
  GapMovXmlUserData   *userDataPtr;

  userDataPtr = (GapMovXmlUserData *)user_data;

  if(gap_debug)
  {
    printf("p_end_xml_element: %s\n", element_name);
  }
  if(userDataPtr == NULL)
  {
    return;
  }

  if(userDataPtr->isScopeValid)
  {
    gint jj;

    for(jj=0; jmpTableElement[jj].name != NULL; jj++)
    {
      if(strcmp(jmpTableElement[jj].name, element_name) == 0)
      {
        jmpTableElement[jj].count++;
      }
    }

  }

}  /* end p_end_xml_element */



/* ------------------------------------------
 * p_transform_path_coordinate
 * ------------------------------------------
 * transforms path coordinate value, typically from recorded size
 * to actual size.
 * Note that the path coordinates in the XML file are stored in unit pixels,
 * refering to the frame image (recordedFrameWidth / recordedFrameHeight) where the move path dialog
 * was invoked from and that typically did write the XML file when
 * move path settings were saved in XML format.
 *
 * This procedure is used to transform the path coordinates to actual frame
 * size at loading time.
 */
static gint
p_transform_path_coordinate(gint value, gint32 recordedSize, gint32 actualSize)
{

  if((recordedSize != 0) && (actualSize != recordedSize))
  {
    gdouble newValue;
    gint    newIntValue;

    newValue = ((gdouble)value * (gdouble)actualSize) / (gdouble)recordedSize;
    newIntValue = rint(newValue);

    return (newIntValue);
  }

  return(value);

}


/* ------------------------------------------
 * p_copy_transformed_values
 * ------------------------------------------
 *
 */
static void
p_copy_transformed_values(GapMovValues *dstValues, GapMovValues *srcValues
   , gint32 actualFrameWidth, gint32 actualFrameHeight)
{
  gint ii;

  if(gap_debug)
  {
    printf("p_copy_transformed_values START\n");
  }

  /* copy all settings, points (this also includes 
   * reallocate the point table in dstValues to same size as the srcValues 
   */
  gap_mov_exec_copy_GapMovValues(dstValues, srcValues);


  /* transform coordinates */
  for(ii=0; ii <= srcValues->point_idx_max; ii++)
  {

    dstValues->point[ii].p_x = p_transform_path_coordinate(srcValues->point[ii].p_x
                                    , srcValues->recordedFrameWidth
                                    , actualFrameWidth
                                    );
    dstValues->point[ii].p_y = p_transform_path_coordinate(srcValues->point[ii].p_y
                                    , srcValues->recordedFrameHeight
                                    , actualFrameHeight
                                    );

  }

}  /* end p_copy_transformed_values */


/* ------------------------------------
 * p_xml_parser_error_handler
 * ------------------------------------
 */
static void
p_error_handler(GMarkupParseContext *context,
                GError              *error,
                gpointer             user_data)
{

  printf("p_xml_parser_error_handler START\n");
  if(error)
  {
    printf("** Error code:%d message:%s\n"
          ,(int)error->code
          ,error->message
          );

  }

  if(context != NULL)
  {
    gint line_number;
    gint char_number;

    g_markup_parse_context_get_position (context, &line_number, &char_number);

    printf("context: line_number:%d char_number:%d element:%s\n"
      ,(int)line_number
      ,(int)char_number
      ,g_markup_parse_context_get_element(context)
      );
  }

  if(user_data != NULL)
  {
    GapMovXmlUserData *userDataPtr;
    userDataPtr = user_data;

    printf("userDataPtr: isParseOk:%d isScopeValid:%d errorLineNumber:%d\n"
      ,(int)userDataPtr->isParseOk
      ,(int)userDataPtr->isScopeValid
      ,(int)userDataPtr->errorLineNumber
      );
  }
  printf("p_xml_parser_error_handler DONE\n");

}  /* end p_xml_parser_error_handler */


/* ------------------------------------------
 * gap_mov_xml_par_load
 * ------------------------------------------
 * (use  actualFrameWidth and actualFrameHeight value 0 in case no transformation
 * is desired)
 */
gboolean
gap_mov_xml_par_load(const char *filename, GapMovValues *productiveValues
    ,gint32 actualFrameWidth, gint32 actualFrameHeight)
{
  /* The list of what handler does what.
   * this implementation uses only start and end element events
   */
  static GMarkupParser parserFuctions = {
    p_start_xml_element,
    p_end_xml_element,
    NULL,
    NULL,
    p_error_handler
  };

  gint jj;
  char *textBuffer;
  gsize lengthTextBuffer;
  gboolean isOk;
  GapMovValues   *tmpValues;
  GapMovXmlUserData *userDataPtr;
  GError            *gError;

  isOk = TRUE;
  gError = NULL;
  tmpValues = gap_mov_exec_new_GapMovValues();
  tmpValues->dst_image_id = productiveValues->dst_image_id;
  tmpValues->rotate_threshold = productiveValues->rotate_threshold;
  userDataPtr = g_new(GapMovXmlUserData, 1);
  userDataPtr->pvals = tmpValues;

  ///p_init_default_values(tmpValues);   /// (?) TODO

  userDataPtr->isScopeValid = FALSE;
  userDataPtr->isParseOk = TRUE;
  userDataPtr->errorLineNumber = 0;

  for(jj=0; jmpTableElement[jj].name != NULL; jj++)
  {
    jmpTableElement[jj].count = 0;
  }

  GMarkupParseContext *context = g_markup_parse_context_new (
        &parserFuctions  /* GMarkupParser */
      , 0                /* GMarkupParseFlags flags */
      , userDataPtr      /* gpointer user_data */
      , NULL             /* GDestroyNotify user_data_dnotify */
      );

  if (g_file_get_contents (filename, &textBuffer, &lengthTextBuffer, NULL) != TRUE)
  {
    printf("Couldn't load XML file:%s\n", filename);
    return(FALSE);
  }


  if (g_markup_parse_context_parse (context, textBuffer, lengthTextBuffer, &gError) != TRUE)
  {
    printf("Parse failed of file: %s\n", filename);
    p_error_handler(context, gError, userDataPtr);

    return(FALSE);
  }

  /* check for mandatory elements */
  if(userDataPtr->isParseOk)
  {
    for(jj=0; jmpTableElement[jj].name != NULL; jj++)
    {
      if(jmpTableElement[jj].isRequired)
      {
        if(jmpTableElement[jj].count < 1)
        {
          printf("required XML element:%s was not found in file:%s\n"
            , jmpTableElement[jj].name
            , filename
            );
          userDataPtr->isParseOk = FALSE;
        }
      }
    }

  }

  if(userDataPtr->isParseOk)
  {
      /* copy loaded values and transform coordinates from recorded frame size
       * to actual frame size
       */
      p_copy_transformed_values(productiveValues, tmpValues, actualFrameWidth, actualFrameHeight);
  }

  isOk = userDataPtr->isParseOk;

  g_free(textBuffer);
  g_markup_parse_context_free (context);

  if(tmpValues->bbp != NULL)
  {
    g_free(tmpValues->bbp);
    tmpValues->bbp = NULL;
  }
  if(tmpValues->src_filename != NULL)
  {
    g_free(tmpValues->src_filename);
    tmpValues->src_filename = NULL;
  }
  gap_mov_exec_free_GapMovValues(tmpValues);

  g_free(userDataPtr);


  return (isOk);

}  /* end gap_mov_xml_par_load */





/*
 * XML WRITER procedure
 */


/* --------------------------------------
 * gap_mov_xml_par_save
 * --------------------------------------
 * save all move path parameter settings to file in XML notation.
 */
gint
gap_mov_xml_par_save(char *filename, GapMovValues *pvals)
{
  FILE *l_fp;
  gint l_idx;

  if(filename == NULL) return -1;

  l_fp = g_fopen(filename, "w+");
  if(l_fp != NULL)
  {
    gboolean isTraceLayerEnabled;
    gboolean writeResizeValues;
    gboolean writeRotateValues;
    gboolean writeOpacityValues;
    gboolean writeFeatherRadiusValues;

    fprintf(l_fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");

    /* root */
    fprintf(l_fp, "<%s ", GAP_MOVPATH_XML_TOKEN_ROOT);
    gap_xml_write_int_value(l_fp, GAP_MOVPATH_XML_TOKEN_VERSION, pvals->version);
    fprintf(l_fp, ">\n");

    /* attributes for description of the processed frames */
    {
      gint32 total_frames;

      total_frames = 1 + abs(pvals->dst_range_end - pvals->dst_range_start);

      fprintf(l_fp, "  <%s ", GAP_MOVPATH_XML_TOKEN_FRAME_DESCRIPTION);
      gap_xml_write_int_value(l_fp, GAP_MOVPATH_XML_TOKEN_WIDTH, gimp_image_width(pvals->dst_image_id));
      gap_xml_write_int_value(l_fp, GAP_MOVPATH_XML_TOKEN_HEIGHT, gimp_image_height(pvals->dst_image_id));
      gap_xml_write_int_value(l_fp, GAP_MOVPATH_XML_TOKEN_RANGE_FROM, pvals->dst_range_start);
      gap_xml_write_int_value(l_fp, GAP_MOVPATH_XML_TOKEN_RANGE_TO, pvals->dst_range_end);
      gap_xml_write_int_value(l_fp, GAP_MOVPATH_XML_TOKEN_TOTAL_FRAMES, total_frames);
      fprintf(l_fp, "/>\n");
    }


    /* attributes for tween processing */
    fprintf(l_fp, "  <%s ", GAP_MOVPATH_XML_TOKEN_TWEEN);
    gap_xml_write_int_value(l_fp, GAP_MOVPATH_XML_TOKEN_TWEEN_STEPS, pvals->tween_steps);
    if(pvals->tween_steps != 0)
    {
      gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_TWEEN_OPACITY_INITIAL, pvals->tween_opacity_initial, 1, 3);
      gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_TWEEN_OPACITY_DESC, pvals->tween_opacity_desc, 1, 3);
    }
    fprintf(l_fp, "/>\n");

    /* attributes for trace layer generation */
    isTraceLayerEnabled = (pvals->tracelayer_enable != 0);
    fprintf(l_fp, "  <%s ", GAP_MOVPATH_XML_TOKEN_TRACE);
    gap_xml_write_gint_as_gboolean_value(l_fp, GAP_MOVPATH_XML_TOKEN_TRACELAYER_ENABLE, isTraceLayerEnabled);
    if(isTraceLayerEnabled)
    {
      gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_TRACE_OPACITY_INITIAL, pvals->trace_opacity_initial, 1, 3);
      gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_TRACE_OPACITY_DESC, pvals->trace_opacity_desc, 1, 3);
    }
    fprintf(l_fp, "/>\n");

    /* attributes of the moving_object */
    {
      char   *src_filename;
      gint32  src_image_id;

      src_image_id = gimp_item_get_image(pvals->src_layer_id);

      fprintf(l_fp, "  <%s ", GAP_MOVPATH_XML_TOKEN_MOVING_OBJECT);
      gap_xml_write_int_value(l_fp, GAP_MOVPATH_XML_TOKEN_SRC_LAYER_ID, pvals->src_layer_id);
      gap_xml_write_int_value(l_fp, GAP_MOVPATH_XML_TOKEN_SRC_LAYERSTACK, pvals->src_layerstack);
      gap_xml_write_int_value(l_fp, GAP_MOVPATH_XML_TOKEN_WIDTH, gimp_image_width(src_image_id));
      gap_xml_write_int_value(l_fp, GAP_MOVPATH_XML_TOKEN_HEIGHT, gimp_image_height(src_image_id));

      src_filename = gimp_image_get_filename(src_image_id);
      if(src_filename != NULL)
      {
        gap_xml_write_string_value(l_fp, GAP_MOVPATH_XML_TOKEN_SRC_FILENAME, src_filename);
        g_free(src_filename);
      }
      fprintf(l_fp, "\n    ");
      gap_xml_write_EnumValue(l_fp, GAP_MOVPATH_XML_TOKEN_SRC_HANDLE, pvals->src_handle, &valuesGapMovHandle[0]);
      fprintf(l_fp, "\n    ");
      gap_xml_write_EnumValue(l_fp, GAP_MOVPATH_XML_TOKEN_SRC_STEPMODE, pvals->src_stepmode, &valuesGapMovStepMode[0]);
      gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_STEP_SPEED_FACTOR, pvals->step_speed_factor, 1, 5);
      fprintf(l_fp, "\n    ");
      gap_xml_write_EnumValue(l_fp, GAP_MOVPATH_XML_TOKEN_SRC_SELMODE, pvals->src_selmode, &valuesGapMovSelMode[0]);
      fprintf(l_fp, "\n    ");
      gap_xml_write_EnumValue(l_fp, GAP_MOVPATH_XML_TOKEN_SRC_PAINTMODE, pvals->src_paintmode, &valuesGimpPaintmode[0]);
      fprintf(l_fp, "\n    ");
      gap_xml_write_int_value(l_fp, GAP_MOVPATH_XML_TOKEN_DST_LAYERSTACK, pvals->dst_layerstack);
      gap_xml_write_string_value(l_fp, GAP_MOVPATH_XML_TOKEN_DST_GROUP_PATH, pvals->dst_group_name_path_string);
      gap_xml_write_string_value(l_fp, GAP_MOVPATH_XML_TOKEN_DST_GROUP_DELIM, pvals->dst_group_name_delimiter);
      gap_xml_write_gint_as_gboolean_value(l_fp, GAP_MOVPATH_XML_TOKEN_SRC_FORCE_VISIBLE, pvals->src_force_visible);
      gap_xml_write_gint_as_gboolean_value(l_fp, GAP_MOVPATH_XML_TOKEN_CLIP_TO_IMG, pvals->clip_to_img);
      gap_xml_write_gint_as_gboolean_value(l_fp, GAP_MOVPATH_XML_TOKEN_SRC_APPLY_BLUEBOX, pvals->src_apply_bluebox);
      fprintf(l_fp, "\n");

      /* attributes for applying bluebox transparency processing */
      if((pvals->src_apply_bluebox) && (pvals->bbp != NULL))
      {
        fprintf(l_fp, "    <%s ", GAP_MOVPATH_XML_TOKEN_BLUEBOX_PARAMETERS);
        fprintf(l_fp, "\n      ");
        gap_xml_write_EnumValue(l_fp, GAP_MOVPATH_XML_TOKEN_THRES_MODE, pvals->bbp->vals.thres_mode, &valuesGapBlueboxThresMode[0]);
        fprintf(l_fp, "\n      ");
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_THRES_R, pvals->bbp->vals.thres_r, 1, 5);
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_THRES_G, pvals->bbp->vals.thres_g, 1, 5);
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_THRES_B, pvals->bbp->vals.thres_b, 1, 5);
        fprintf(l_fp, "\n      ");
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_THRES_H, pvals->bbp->vals.thres_h, 1, 5);
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_THRES_S, pvals->bbp->vals.thres_s, 1, 5);
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_THRES_V, pvals->bbp->vals.thres_v, 1, 5);
        fprintf(l_fp, "\n      ");
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_THRES, pvals->bbp->vals.thres, 1, 5);
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_TOLERANCE, pvals->bbp->vals.tolerance, 1, 5);
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_GROW, pvals->bbp->vals.grow, 1, 5);
        fprintf(l_fp, "\n      ");
        gap_xml_write_gint_as_gboolean_value(l_fp, GAP_MOVPATH_XML_TOKEN_FEATHER_EDGES, pvals->bbp->vals.feather_edges);
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_FEATHER_RADIUS, pvals->bbp->vals.feather_radius, 1, 3);
        fprintf(l_fp, "\n      ");
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_SOURCE_ALPHA, pvals->bbp->vals.source_alpha, 1, 3);
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_TARGET_ALPHA, pvals->bbp->vals.target_alpha, 1, 3);
        fprintf(l_fp, "\n      ");
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_KEYCOLOR_R, pvals->bbp->vals.keycolor.r, 1, 3);
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_KEYCOLOR_G, pvals->bbp->vals.keycolor.g, 1, 3);
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_KEYCOLOR_B, pvals->bbp->vals.keycolor.b, 1, 3);

        fprintf(l_fp, "\n");
        fprintf(l_fp, "      >\n");
        fprintf(l_fp, "    </%s>\n", GAP_MOVPATH_XML_TOKEN_BLUEBOX_PARAMETERS);

      }

      fprintf(l_fp, "    >\n");
      fprintf(l_fp, "  </%s>\n", GAP_MOVPATH_XML_TOKEN_MOVING_OBJECT);
    }

    fprintf(l_fp, "\n");

    /* controlpoints */

    fprintf(l_fp, "  <%s ", GAP_MOVPATH_XML_TOKEN_CONTROLPOINTS);
    gap_xml_write_int_value(l_fp, GAP_MOVPATH_XML_TOKEN_CURRENT_POINT, pvals->point_idx);
    gap_xml_write_int_value(l_fp, GAP_MOVPATH_XML_TOKEN_NUMBER_OF_POINTS, pvals->point_idx_max +1);
    gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_ROTATE_THRESHOLD, pvals->rotate_threshold, 1, 7);
    fprintf(l_fp, " >\n");

    /* check for conditonal write
     * (to keep files smaller and more readable we skip writing of some information
     * that is equal to the default value in all controlpoints)
     */
    writeResizeValues = FALSE;
    writeRotateValues = FALSE;
    writeOpacityValues = FALSE;
    writeFeatherRadiusValues = FALSE;
    for(l_idx = 0; l_idx <= pvals->point_idx_max; l_idx++)
    {
      if((pvals->point[l_idx].w_resize != 100.0)
      || (pvals->point[l_idx].h_resize != 100.0))
      {
        writeResizeValues = TRUE;
      }

      if(pvals->point[l_idx].opacity != 100.0)
      {
        writeOpacityValues = TRUE;
      }

      if(pvals->point[l_idx].rotation != 0.0)
      {
        writeRotateValues = TRUE;
      }

      if(pvals->point[l_idx].sel_feather_radius != 0.0)
      {
        writeFeatherRadiusValues = TRUE;
      }

    }

    /* write controlpoints loop */
    for(l_idx = 0; l_idx <= pvals->point_idx_max; l_idx++)
    {
      gdouble  px;
      gdouble  py;
      gboolean writeAccelerationCharacteristics;
      gboolean keyframeInNewLine;

      keyframeInNewLine = FALSE;
      px = pvals->point[l_idx].p_x;
      py = pvals->point[l_idx].p_y;

      /* write basic attributes of the controlpoint */

      fprintf(l_fp, "    <%s ", GAP_MOVPATH_XML_TOKEN_CONTROLPOINT);
      gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_PX, px, 5, 0);
      gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_PY, py, 5, 0);
      if(writeResizeValues)
      {
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_W_RESIZE, pvals->point[l_idx].w_resize, 3, 3);
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_H_RESIZE, pvals->point[l_idx].h_resize, 3, 3);
      }
      if(writeOpacityValues)
      {
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_OPACITY, pvals->point[l_idx].opacity, 3, 1);
      }
      if(writeRotateValues)
      {
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_ROTATION, pvals->point[l_idx].rotation, 4, 1);
      }
      if(writeFeatherRadiusValues)
      {
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_SEL_FEATHER_RADIUS, pvals->point[l_idx].sel_feather_radius, 3, 1);
      }

      /* conditional write perspective transformation (only if there is any) */
      if(pvals->point[l_idx].ttlx != 1.0
            || pvals->point[l_idx].ttly != 1.0
            || pvals->point[l_idx].ttrx != 1.0
            || pvals->point[l_idx].ttry != 1.0
            || pvals->point[l_idx].tblx != 1.0
            || pvals->point[l_idx].tbly != 1.0
            || pvals->point[l_idx].tbrx != 1.0
            || pvals->point[l_idx].tbry != 1.0
            )
      {
        keyframeInNewLine = TRUE;
        fprintf(l_fp, "\n      ");
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_TTLX, pvals->point[l_idx].ttlx, 2, 3);
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_TTLY, pvals->point[l_idx].ttly, 2, 3);
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_TTRX, pvals->point[l_idx].ttrx, 2, 3);
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_TTRY, pvals->point[l_idx].ttry, 2, 3);
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_TBLX, pvals->point[l_idx].tblx, 2, 3);
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_TBLY, pvals->point[l_idx].tbly, 2, 3);
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_TBRX, pvals->point[l_idx].tbrx, 2, 3);
        gap_xml_write_gdouble_value(l_fp, GAP_MOVPATH_XML_TOKEN_TBRY, pvals->point[l_idx].tbry, 2, 3);
      }

      writeAccelerationCharacteristics = FALSE;
      /* conditional write acceleration characteristics
       * relevant information can occur at first controlpoint
       * or on keyframes but never for the last controlpoint)
       */
      if ((pvals->point[l_idx].accPosition != 0)
      ||  (pvals->point[l_idx].accOpacity != 0)
      ||  (pvals->point[l_idx].accSize != 0)
      ||  (pvals->point[l_idx].accRotation != 0)
      ||  (pvals->point[l_idx].accPerspective != 0)
      ||  (pvals->point[l_idx].accSelFeatherRadius != 0))
      {
        if (l_idx == 0)
        {
          writeAccelerationCharacteristics = TRUE;
        }
        else
        {
          if ((l_idx < pvals->point_idx_max)
          && ((int)pvals->point[l_idx].keyframe > 0))
          {
            writeAccelerationCharacteristics = TRUE;
          }
        }

      }

      if (writeAccelerationCharacteristics == TRUE)
      {
        keyframeInNewLine = TRUE;
        fprintf(l_fp, "\n      ");
        gap_xml_write_int_value(l_fp, GAP_MOVPATH_XML_TOKEN_ACC_POSITION, pvals->point[l_idx].accPosition);
        gap_xml_write_int_value(l_fp, GAP_MOVPATH_XML_TOKEN_ACC_OPACITY, pvals->point[l_idx].accOpacity);
        gap_xml_write_int_value(l_fp, GAP_MOVPATH_XML_TOKEN_ACC_SIZE, pvals->point[l_idx].accSize);
        gap_xml_write_int_value(l_fp, GAP_MOVPATH_XML_TOKEN_ACC_ROTATION, pvals->point[l_idx].accRotation);
        gap_xml_write_int_value(l_fp, GAP_MOVPATH_XML_TOKEN_ACC_PERSPECTIVE, pvals->point[l_idx].accPerspective);
        gap_xml_write_int_value(l_fp, GAP_MOVPATH_XML_TOKEN_ACC_SEL_FEATHER_RADIUS, pvals->point[l_idx].accSelFeatherRadius);
      }

      if(gap_debug)
      {
        printf("idx:%d point_idx_max:%d keyframe:%d keyframe_abs:%d to_keyframe:%d\n"
          ,l_idx
          ,pvals->point_idx_max
          ,pvals->point[l_idx].keyframe
          ,pvals->point[l_idx].keyframe_abs
          ,gap_mov_exec_conv_keyframe_to_rel(pvals->point[l_idx].keyframe_abs, pvals)
          );
      }

      /* check for writing keyframe
       * (the implicite keyframes at first and last controlpoints are not written to file)
       */
      if((l_idx > 0)
      && (l_idx < pvals->point_idx_max)
      && ((int)pvals->point[l_idx].keyframe_abs > 0))
      {
        if(keyframeInNewLine == TRUE)
        {
          fprintf(l_fp, "\n      ");
        }
        gap_xml_write_int_value(l_fp, GAP_MOVPATH_XML_TOKEN_KEYFRAME_ABS,
                                pvals->point[l_idx].keyframe_abs);

      }

      fprintf(l_fp, "/>\n"); /* end of GAP_MOVPATH_XML_TOKEN_CONTROLPOINT */

    }

    fprintf(l_fp, "  </%s>\n", GAP_MOVPATH_XML_TOKEN_CONTROLPOINTS);
    fprintf(l_fp, "</%s>", GAP_MOVPATH_XML_TOKEN_ROOT);

    fclose(l_fp);
    return 0;
  }
  return -1;
}  /* end gap_mov_xml_par_save */

