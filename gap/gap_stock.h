/*
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 *
 * Copyright (C) 1997 Andy Thomas  <alt@picnic.demon.co.uk>
 *               2003 Sven Neumann  <sven@gimp.org>
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

#ifndef __GAP_STOCK_H__
#define __GAP_STOCK_H__

#define GAP_STOCK_ADD_POINT          "gap-add-point"
#define GAP_STOCK_ANIM_PREVIEW       "gap-anim-preview"
#define GAP_STOCK_DELETE_ALL_POINTS  "gap-delete-all-points"
#define GAP_STOCK_DELETE_POINT       "gap-delete-point"
#define GAP_STOCK_FIRST_POINT        "gap-first-point"
#define GAP_STOCK_GRAB_POINTS        "gap-grab-points"
#define GAP_STOCK_INSERT_POINT       "gap-insert-point"
#define GAP_STOCK_LAST_POINT         "gap-last-point"
#define GAP_STOCK_NEXT_POINT         "gap-next-point"
#define GAP_STOCK_PAUSE              "gap-pause"
#define GAP_STOCK_PLAY               "gap-play"
#define GAP_STOCK_PLAY_REVERSE       "gap-play-reverse"
#define GAP_STOCK_PREV_POINT         "gap-prev-point"
#define GAP_STOCK_RESET_ALL_POINTS   "gap-reset-all-points"
#define GAP_STOCK_RESET_POINT        "gap-reset-point"
#define GAP_STOCK_ROTATE_FOLLOW      "gap-rotate-follow"
#define GAP_STOCK_SOURCE_IMAGE       "gap-source-image"
#define GAP_STOCK_STEPMODE           "gap-stepmode"
#define GAP_STOCK_UPDATE             "gap-update"

#define GAP_STOCK_RANGE_END          "gap-range-end"
#define GAP_STOCK_RANGE_START        "gap-range-start"
#define GAP_STOCK_SET_RANGE_END      "gap-set-range-end"
#define GAP_STOCK_SET_RANGE_START    "gap-set-range-start"
#define GAP_STOCK_SPEED              "gap-speed"


#include <gtk/gtk.h>

void  gap_stock_init (void);
GtkWidget *  gap_stock_button_new(const char *stock_id);
GtkWidget *  gap_stock_button_new_with_label(const char *stock_id, const char *optional_label);

#endif /* __GAP_STOCK_H__ */
