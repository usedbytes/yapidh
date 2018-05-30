/*
 * step_gen.h Stepper motor step profile generator
 * Copyright (c) 2018 Brian Starkey <stark3y@gmail.com>
 *
 * This implements David Austin's algorithm for real-time stepper motor
 * acceleration profile generation.
 * See: https://www.embedded.com/design/mcus-processors-and-socs/4006438/Generate-stepper-motor-speed-profiles-in-real-time
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
#ifndef __STEP_GEN_H__
#define __STEP_GEN_H__

struct step_ctx {
	double alpha;
	double accel;
	double f;

	double n;
	double target_n;

	double c;
};

void stepper_set_speed(struct step_ctx *c, double speed);
void stepper_tick(struct step_ctx *c);
void step_ctx_dump(struct step_ctx *c);
void step_ctx_init(struct step_ctx *ctx, int steps_per_rev, double timer_freq,
		   double accel_radss);

#endif /* __STEP_GEN_H__ */
