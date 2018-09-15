/*
 * stepper_driver.h Event source for stepper motor
 * Copyright (c) 2018 Brian Starkey <stark3y@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __STEP_SOURCE_H__
#define __STEP_SOURCE_H__

#include "wave_gen.h"

struct source *stepper_create(int step, int dir, int pwdn);
void stepper_set_velocity(struct source *s, double rads);
int32_t stepper_get_steps(struct source *s);

#endif /* __STEPPER_DRIVER_H__ */
