/*
 * wave_gen.c Event sequence generator
 * Copyright (c) 2018 Brian Starkey <stark3y@gmail.com>
 *
 * Given a set of event sources, capable of generating events at discrete
 * intervals, generate sequences of delays and events
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
#include <assert.h>
#include "wave_gen.h"

#define MAX_SOURCES 4

void wave_gen(struct wave_ctx *c, int budget)
{
	int i, min;

	if (c->be->start_wave) {
		c->be->start_wave(c->be);
	}

	while (budget) {
		min = budget;

		for (i = 0; i < c->n_sources; i++) {
			if (c->t[i] == 0) {
				/* This source is ready to generate an event */
				struct source *s = c->sources[i];

				/* Generate the event, and get the time to the next one */
				c->t[i] = c->be->add_event(c->be, s);
				assert(c->t[i] > 0);
			}

			/* Keep track of when the next event is */
			if (c->t[i] < min) {
				min = c->t[i];
			}
		}

		/* Delay until the next event */
		c->be->add_delay(c->be, min);

		/* Reduce all the remaining delays */
		for (i = 0; i < c->n_sources; i++) {
			c->t[i] -= min;
		}

		/* Reduce the remaining time available in this chunk */
		budget -= min;
	}

	if (c->be->end_wave) {
		c->be->end_wave(c->be);
	}
}
