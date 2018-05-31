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

#include "vcd_backend.h"
#include "types.h"

static void vcd_backend_add_event(struct wave_backend *wb, struct source *s)
{
	struct vcd_backend *be = (struct vcd_backend *)wb;
	struct event ev;

	s->gen_event(s, &ev);

	switch (ev.type) {
	case EVENT_RISING_EDGE:
		be->rising |= (1 << ev.channel);
		break;
	case EVENT_FALLING_EDGE:
		be->falling |= (1 << ev.channel);
		break;
	}
}

static void vcd_backend_add_delay(struct wave_backend *wb, int delay)
{
	struct vcd_backend *be = (struct vcd_backend *)wb;
	int i;

	printf("#%d ", be->time);
	for (i = 0; i < be->n_channels; i++) {
		if (be->rising & (1 << i)) {
			printf("1%c ", '!' + i);
		}
		if (be->falling & (1 << i)) {
			printf("0%c ", '!' + i);
		}
	}
	printf("\n");

	be->rising = be->falling = 0;
	be->time += delay;
}

struct vcd_backend *vcd_backend_create(int n_channels, const char *names[])
{
	struct vcd_backend *be = calloc(1, sizeof(*be));
	int i;

	be->base.add_delay = vcd_backend_add_delay;
	be->base.add_event = vcd_backend_add_event;

	be->n_channels = n_channels;
	printf("$timescale 10 us $end\n");
	for (i = 0; i < be->n_channels; i++) {
		printf("$var wire 1 %c %s $end\n", '!' + i, names[i]);
	}
	printf("$enddefinitions $end\n");


	return be;
}
