/*
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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "gnuplot_backend.h"

static void gnuplot_backend_add_event(struct wave_backend *wb, struct source *s)
{
	struct gnuplot_backend *gb = (struct gnuplot_backend *)wb;
	struct event ev;

	s->gen_event(s, &ev);

	switch (ev.type) {
	case EVENT_RISING_EDGE:
		gb->state |= (1 << ev.channel);
		break;
	case EVENT_FALLING_EDGE:
		gb->state &= ~(1 << ev.channel);
		break;
	}
}

static void print_state(int time, uint32_t state)
{
	int i;
	printf("%d, ", time);
	for (i = 0; i < 4; i++) {
		printf("%c, ", state & (1 << i) ? '1' : '0');
	}
	printf("\n");
}

static void gnuplot_backend_add_delay(struct wave_backend *wb, int delay)
{
	struct gnuplot_backend *gb = (struct gnuplot_backend *)wb;

	if (gb->prev_time < gb->time - 1) {
		print_state(gb->time - 1, gb->prev_state);
	}
	print_state(gb->time, gb->state);

	gb->prev_time = gb->time;
	gb->prev_state = gb->state;
	gb->time += delay;
}

struct gnuplot_backend *gnuplot_backend_create()
{
	struct gnuplot_backend *gb = calloc(1, sizeof(*gb));

	gb->base.add_delay = gnuplot_backend_add_delay;
	gb->base.add_event = gnuplot_backend_add_event;

	return gb;
}
