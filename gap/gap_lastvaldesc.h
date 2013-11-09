/*  gap_lastvaldesc.h
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
 *          2002/09/21   hof: created.
 */

#ifndef __GAP_LASTVALDESC_H__
#define __GAP_LASTVALDESC_H__

/* gimplastvaldesc should become part of libgimp in the future.
 * if this will come true, HAVE_LASTVALDESC_H  will also become DEFINED.
 */
#ifdef HAVE_LASTVALDESC_H
#include "libgimp/gimp.h"
#include "libgimp/gimplastvaldesc.h"
#else
  /* for now GAP Sources include a private version of gimplastvaldesc.h gimplastvaldesc.c Modules
   * and must include them explicite because HAVE_LASTVALDESC_H is not defined
   * (i dont know, if gimplastvaldesc will be a part of libgimp someday -- hof)
   */
#include "libgimp/gimp.h"
#include "gimplastvaldesc.h"   
#endif

#endif  /* __GAP_LASTVALDESC_H__ */
