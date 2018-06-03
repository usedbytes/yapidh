/*
 * step_gen.c Stepper motor step profile generator
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
#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "step_gen.h"

static double same_sign(double a, double b)
{
	if (b < 0) {
		return a < 0 ? a : -a;
	}
	return a >= 0 ? a : -a;
}

void stepper_set_speed(struct step_ctx *c, double speed)
{
	c->steady = 0;
	c->speed = speed;
	c->target_n = (speed * speed) / (2 * c->alpha * c->accel);
	if (c->target_n < fabs(c->n)) {
		c->target_n = -c->target_n;
	}

	c->n = same_sign(c->n, c->target_n);
}

void stepper_tick(struct step_ctx *c)
{
	if (c->n == 0.0f) {
		c->c = 0.676 * c->f * sqrt((2 * c->alpha) / c->accel);
		c->n = 1;
		return;
	}

	if (c->n >= c->target_n) {
		if (c->target_n == 0.0f) {
			// Disable motor.
		} else if (!c->steady) {
			c->c = (c->alpha * c->f) / c->speed;
			c->steady = 1;
		}

		return;
	}

	c->c = c->c - ((2 * c->c) / ((4 * c->n) + 1));
	c->n++;
}

void step_ctx_dump(struct step_ctx *c)
{
	fprintf(stderr, "------\n");
	fprintf(stderr, "Alpha   : %4.3f\n", c->alpha);
	fprintf(stderr, "Accel   : %4.3f\n", c->accel);
	fprintf(stderr, "Freq    : %4.3f\n", c->f);
	fprintf(stderr, "n       : %4.3f\n", c->n);
	fprintf(stderr, "target_n: %4.3f\n", c->target_n);
	fprintf(stderr, "c       : %4.3f\n", c->c);
	fprintf(stderr, "speed   : %4.3f\n", (c->f * c->alpha) / round(c->c));
	fprintf(stderr, "------\n");
}

void step_ctx_init(struct step_ctx *ctx, int steps_per_rev, double timer_freq,
		   double accel_radss)
{
	ctx->alpha = (2 * M_PI) / steps_per_rev;
	ctx->f = timer_freq;
	ctx->accel = accel_radss;
}
