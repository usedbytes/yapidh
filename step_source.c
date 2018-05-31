/*
 * step_source.c Event source for stepper motor
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

#include "gnuplot_backend.h"
#include "step_source.h"
#include "types.h"

static int step_source_get_delay(struct source *s)
{
	struct step_source *ss = (struct step_source *)s;

	if (ss->edge == EDGE_RISING) {
		stepper_tick(&ss->sctx);
		ss->gap = round(ss->sctx.c);
		ss->edge = EDGE_FALLING;
		return ss->pulsewidth;
	} else {
		ss->edge = EDGE_RISING;
		return ss->gap - ss->pulsewidth;
	}
}

static void step_source_gen_event(struct source *s, struct event *ev)
{
	struct step_source *ss = (struct step_source *)s;

	if (ss->edge == EDGE_RISING) {
		ev->type = EVENT_RISING_EDGE;
		ev->channel = 0;
	} else {
		ev->type = EVENT_FALLING_EDGE;
		ev->channel = 0;
	}
}

struct step_source *step_source_create()
{
	struct step_source *ss = calloc(1, sizeof(*ss));

	ss->base.gen_event = step_source_gen_event;
	ss->base.get_delay = step_source_get_delay;
	ss->pulsewidth = 5;

	step_ctx_init(&ss->sctx, 600, 100000, 100);

	return ss;
}
