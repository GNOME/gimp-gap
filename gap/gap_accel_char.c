/*  gap_accel_char.c
 *
 *  This module handles GAP acceleration characteristics calculation.
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
 * version 2.7.0; 2010/02/06  hof: created
 */

#include "config.h"

#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include "gap_accel_char.h"


/* ---------------------------------------
 * gap_accelMixFactor
 * ---------------------------------------
 * this procedure implements hardcoded acceleration characteristics.
 *     
 * accelCharacteristic: 
 *  0 is used to turn off acceleration
 *  1 specified constant speed
 *
 * positive values > 1 represent acceleration, 
 * negative values < -1  for deceleration
 *
 * orig_factor: a positive gdouble in the range 0.0 to 1.0
 * returns modified mix_factor in the range 0.0 to 1.0 according to specified accelCharacteristic
 * the returned value is equal to orig_factor for accelCharacteristic -1, 0 amd 1
 */
gdouble 
gap_accelMixFactor(gdouble orig_factor, gint accelCharacteristic)
{
  gdouble mix;
  gdouble accelPower;
  
  switch (accelCharacteristic)
  {
    case 0:
    case 1:
    case -1:
      mix = orig_factor;
      break;
    default:
      if (accelCharacteristic > 0)
      {
        accelPower = 1 + ((accelCharacteristic -1) / 10.0);
        mix = pow(orig_factor, accelPower);
      }
      else
      {
        accelPower = 1 + (((0 - accelCharacteristic) -1) / 10.0);
        mix = 1.0 - pow((1.0 - orig_factor), accelPower);
      }
      break;
  }

  return (mix);  
}  /* end gap_accelMixFactor */


/* ---------------------------------------
 * gap_accel_calculate_current_step
 * ---------------------------------------
 * calculate current step respecting the specified acceleration characteristic
 */
gdouble
gap_calculate_current_step_with_acceleration(gdouble current_step, gint32 total_steps, gint accelCharacteristic)
{
  gdouble currentStepClamped;
  gdouble accelStep;

  if (total_steps <= 0)
  {
    return (current_step);
  }
  currentStepClamped = CLAMP(current_step, 0.0, (gdouble)total_steps);
  
  switch (accelCharacteristic)
  {
    case 0:
    case 1:
    case -1:
      return (currentStepClamped);
      break;
    default:
      accelStep = gap_accelMixFactor(currentStepClamped / (gdouble)total_steps, accelCharacteristic);
      break;
  }

  return (accelStep * (gdouble)total_steps);

}  /* end gap_calculate_current_step_with_acceleration */



