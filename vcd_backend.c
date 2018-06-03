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
#include <unistd.h>

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

static char get_id(struct vcd_backend *be, int pin)
{
	int i;
	for (i = 0; i < be->n_channels; i++) {
		if (pin == be->pins[i]) {
			return '!' + i;
		}
	}

	fprintf(stderr, "Unknown pin %d\n", pin);
	return '\0';
}

static void vcd_backend_add_delay(struct wave_backend *wb, int delay)
{
	struct vcd_backend *be = (struct vcd_backend *)wb;
	int i;

	printf("#%d ", be->time);
	for (i = 0; i < 32; i++) {
		if (be->rising & (1 << i)) {
			printf("1%c ", get_id(be, i));
		}
		if (be->falling & (1 << i)) {
			printf("0%c ", get_id(be, i));
		}
	}
	printf("\n");

	be->rising = be->falling = 0;
	be->time += delay;
}

void vcd_backend_fini(struct vcd_backend *be)
{
	free(be);
}

struct vcd_backend *vcd_backend_create(uint32_t pins)
{
	struct vcd_backend *be = calloc(1, sizeof(*be));
	int i, n;

	be->base.add_delay = vcd_backend_add_delay;
	be->base.add_event = vcd_backend_add_event;

	for (i = 0; i < 32; i++) {
		if (pins & (1 << i)) {
			be->n_channels++;
		}
	}
	be->pins = calloc(be->n_channels, sizeof(*be->pins));

	printf("$timescale 10 us $end\n");

	n = 0;
	for (i = 0; i < 32; i++) {
		if (pins & (1 << i)) {
			be->pins[n] = i;
			printf("$var wire 1 %c pin%d $end\n", '!' + n, i);
			n++;
		}
	}

	printf("$enddefinitions $end\n");

	return be;
}

struct platform {
	struct vcd_backend *be;
};

void platform_fini(struct platform *p)
{
	if (p->be) {
		vcd_backend_fini(p->be);
	}
	free(p);
}

struct platform *platform_init(uint32_t pins)
{
	struct platform *p = calloc(1, sizeof(*p));
	if (!p) {
		return NULL;
	}

	p->be = vcd_backend_create(pins);
	if (!p->be) {
		goto fail;
	}

	return p;

fail:
	platform_fini(p);
	return NULL;
}

struct wave_backend *platform_get_backend(struct platform *p)
{
	return (struct wave_backend *)p->be;
}

int platform_sync(struct platform *p, int timeout_millis)
{
	// Just sleep so the output is a bit rate limited
	usleep(100000);
	return 0;
}
