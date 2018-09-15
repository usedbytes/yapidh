/*
 * stepper_driver.c Event source for stepper motor
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
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "stepper_driver.h"
#include "types.h"

#define F_COUNT 100000
#define COUNT_1MS 100

#define F_COUNT 100000
#define COUNT_1MS 100

enum edge {
	EDGE_RISING,
	EDGE_FALLING,
};

struct leib_ctx {
	double alpha;
	double accel;
	double freq;

	double r;

	double v0;
	double v;

	double p1;
	double p;
	double ps;
	double p0;
	double m;
};

static bool leib_stopped(struct leib_ctx *c)
{
	return c->m == 0.0f && c->v == 0.0f;
}

static void leib_init(struct leib_ctx *c, int steps_per_rev, double timer_freq,
		   double accel_radss)
{
	c->alpha = (2 * M_PI) / steps_per_rev;
	c->freq = timer_freq;
	c->accel = accel_radss / c->alpha;

	c->r = c->accel / (c->freq * c->freq);

	c->p0 = c->freq / sqrt(0 * 0 + 2 * c->accel);
}

static void leib_start_segment(struct leib_ctx *c, double v)
{
	v = v / c->alpha;

	if (leib_stopped(c)) {
		c->v0 = 0.0f;
	} else {
		c->v0 = c->freq / c->p;
	}
	c->v = v;
	c->p1 = c->freq / sqrt(c->v0 * c->v0 + 2 * c->accel);
	c->ps = v == 0 ? c->p0 : c->freq / c->v;

	if (c->v < c->v0) {
		c->m = c->r;
	} else if (c->v > c->v0) {
		c->m = -c->r;
	} else {
		c->m = 0;
	}

	c->p = c->p1;
}

static double leib_tick(struct leib_ctx *c)
{
	double p = c->p;

	if (c->m < 0 && c->p < c->ps) {
		p = c->p = c->ps;
		c->m = 0;
	} else if (c->m > 0 && c->p > c->ps) {
		p = c->p = c->ps;
		c->m = 0;
	} else if (c->m != 0) {
		p = c->p;
		c->p = c->p * (1 + c->m * c->p * c->p);
	}

	return p;
}

enum stepper_state {
	STATE_STOPPED,
	STATE_FWD,
	STATE_REV,
};

struct stepper_motor {
	struct source base;

	int edge;
	int gap;
	int pulsewidth;
	int channel;

	uint8_t step_pin;
	uint8_t dir_pin;
	uint8_t pwdn_pin;

	uint32_t falling, rising;

	enum stepper_state state;
	double target_rads;

	struct leib_ctx ctrl;

	int32_t steps;
	int dsteps;
};

void stepper_set_velocity(struct source *s, double rads)
{
	struct stepper_motor *m = (struct stepper_motor *)s;

	m->target_rads = rads;
	if (((m->state == STATE_FWD) && (rads <= 0.0f)) ||
	    ((m->state == STATE_REV) && (rads >= 0.0f))) {
		// Changing direction - go through 0 first
		rads = 0.0f;
	}

	leib_start_segment(&m->ctrl, fabs(rads));
}

int32_t stepper_get_steps(struct source *s)
{
	struct stepper_motor *m = (struct stepper_motor *)s;
	int32_t ret = m->steps;
	m->steps = 0;
	return ret;
}

static uint32_t stepper_gen_event(struct source *s, struct event *ev)
{
	struct stepper_motor *m = (struct stepper_motor *)s;
	double c;

	/* First check if we're stopped, and if so, just sleep */
	if (m->state == STATE_STOPPED && m->target_rads == 0.0f) {
		ev->rising |= (1 << m->pwdn_pin);
		return 5 * COUNT_1MS;
	}

	/* Next, check for falling edge */
	if (m->gap) {
		ev->falling |= (1 << m->step_pin);

		c = m->gap;
		m->gap = 0;

		return c - m->pulsewidth;
	}

	/* Otherwise, get the next rising edge */
	c = leib_tick(&m->ctrl);

	/* If we're currently stopped, but not meant to be, start moving */
	if (m->state == STATE_STOPPED) {
		if (m->target_rads > 0) {
			m->state = STATE_FWD;
			ev->rising |= (1 << m->dir_pin);
			m->dsteps = 1;
		} else {
			m->state = STATE_REV;
			ev->falling |= (1 << m->dir_pin);
			m->dsteps = -1;
		}
		ev->falling |= (1 << m->pwdn_pin);
	} else if (leib_stopped(&m->ctrl)) {
		/* We're stopped, but it could be a zero crossing */
		m->state = STATE_STOPPED;

		if (m->target_rads == 0.0f) {
			/* Really stopped, power down the motor */
			ev->rising |= (1 << m->pwdn_pin);
		}

		/* If we're zero-crossing this will get us to the final speed */
		stepper_set_velocity(s, m->target_rads);

		/* Recurse once, to either sleep, or generate the first pulse */
		return stepper_gen_event(s, ev);
	}

	/*
	 * Setting enable and sending the first pulse at the same
	 * time might be a bad idea, but otherwise we need another
	 * state, so let's try like this for now
	 */
	ev->rising |= (1 << m->step_pin);
	m->steps += m->dsteps;

	m->gap = round(c);
	return m->pulsewidth;
}

struct source *stepper_create(int step, int dir, int pwdn)
{
	struct stepper_motor *m = calloc(1, sizeof(*m));

	m->base.gen_event = stepper_gen_event;
	m->pulsewidth = 5;
	m->step_pin = step;
	m->dir_pin = dir;
	m->pwdn_pin = pwdn;
	m->steps = 0;
	m->dsteps = 0;

	leib_init(&m->ctrl, 600, F_COUNT, 200);

	m->rising = (1 << m->pwdn_pin);

	return &m->base;
}
