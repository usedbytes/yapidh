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
#include <stdlib.h>

#include "gnuplot_backend.h"

static void gnuplot_backend_add_event(struct wave_backend *wb, struct source *s)
{
	struct gnuplot_backend *gb = (struct gnuplot_backend *)wb;

	if (gb->time > 0) {
		printf("%d, %d\n", gb->time - 1, gb->ev.val);
	}

	s->gen_event(s, &gb->ev);
	printf("%d, %d\n", gb->time, gb->ev.val);
}

static void gnuplot_backend_add_delay(struct wave_backend *wb, int delay)
{
	struct gnuplot_backend *gb = (struct gnuplot_backend *)wb;

	gb->time += delay;
}

struct gnuplot_backend *gnuplot_backend_create()
{
	struct gnuplot_backend *gb = calloc(1, sizeof(*gb));

	gb->base.add_delay = gnuplot_backend_add_delay;
	gb->base.add_event = gnuplot_backend_add_event;

	return gb;
}
