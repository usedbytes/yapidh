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

struct speed_ctrl {
	double alpha;
	double accel;
	double f;

	double n;
	double target_n;

	double c;
	double set_speed;

	int steady;
};

void speed_ctrl_init(struct speed_ctrl *c, int steps_per_rev, double timer_freq,
		   double accel_radss)
{
	c->alpha = (2 * M_PI) / steps_per_rev;
	c->f = timer_freq;
	c->accel = accel_radss;
}

static double same_sign(double a, double b)
{
	if (signbit(b)) {
		return signbit(a) ? a : -a;
	}
	return signbit(a) ? -a : a;
}

// speed should be positive.
static void speed_ctrl_set(struct speed_ctrl *c, double speed)
{
	double target_n = (speed * speed) / (2 * c->alpha * c->accel);
	double n = c->n;
	if (target_n < fabs(c->n)) {
		target_n = target_n > 0.0f ? -target_n : 0.0f;
		n = same_sign(n, -1);
	} else {
		n = same_sign(n, 1);
	}

	c->steady = 0;
	c->set_speed = speed;
	c->target_n = target_n;
	c->n = n;
}

// Returns:
//  0 - stopped
//  Otherwise, delay
static int speed_ctrl_tick(struct speed_ctrl *c)
{
	if (c->n == 0.0f) {
		if (c->target_n != 0.0f) {
			// Need to start turning
			c->c = 0.676 * c->f * sqrt((2 * c->alpha) / c->accel);
			c->n = 1;
			return c->c;
		}

		return 0;
	}

	if (c->n < c->target_n - 1.0f) {
		c->n++;
		c->c = c->c - ((2 * c->c) / ((4 * c->n) + 1));
	} else if (!c->steady) {
		if (c->set_speed != 0.0f) {
			// Calculate exact count to prevent accumulated error
			c->c = (c->alpha * c->f) / c->set_speed;
		} else {
			c->c = 0.0f;
		}

		c->steady = 1;
		c->n = c->target_n;
	}

	return round(c->c);
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

	struct speed_ctrl ctrl;
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

	speed_ctrl_set(&m->ctrl, fabs(rads));
}

static uint32_t stepper_gen_event(struct source *s, struct event *ev)
{
	struct stepper_motor *m = (struct stepper_motor *)s;
	int c;

	/* First check if we're stopped, and if so, just sleep */
	if (m->state == STATE_STOPPED && m->target_rads == 0.0f) {
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
	c = speed_ctrl_tick(&m->ctrl);
	if (c) {
		if (m->state == STATE_STOPPED) {
			/* First pulse (or zero crossing), set direction and enable motor */
			if (m->target_rads > 0) {
				m->state = STATE_FWD;
				ev->rising |= (1 << m->dir_pin);
			} else {
				m->state = STATE_REV;
				ev->falling |= (1 << m->dir_pin);
			}
			ev->falling |= (1 << m->pwdn_pin);
		}

		/*
		 * Setting enable and sending the first pulse at the same
		 * time might be a bad idea, but otherwise we need another
		 * state, so let's try like this for now
		 */
		ev->rising |= (1 << m->step_pin);

		m->gap = c;
		return m->pulsewidth;
	} else {
		/* We're stopped, but could be a zero crossing */
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
}

struct source *stepper_create(int step, int dir, int pwdn)
{
	struct stepper_motor *m = calloc(1, sizeof(*m));

	m->base.gen_event = stepper_gen_event;
	m->pulsewidth = 5;
	m->step_pin = step;
	m->dir_pin = dir;
	m->pwdn_pin = pwdn;

	speed_ctrl_init(&m->ctrl, 600, F_COUNT, 100);

	m->rising = (1 << m->pwdn_pin);

	return &m->base;
}
