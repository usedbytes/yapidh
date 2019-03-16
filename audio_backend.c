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

#include "audio_backend.h"
#include "types.h"

static uint32_t audio_backend_add_event(struct wave_backend *wb, struct source *s)
{
	struct audio_backend *be = (struct audio_backend *)wb;
	struct event ev = { 0 };
	uint32_t ret;

	ret = s->gen_event(s, &ev);

	be->state |= ev.rising;
	be->state &= ~ev.falling;

	return ret;
}

static void audio_backend_add_delay(struct wave_backend *wb, int delay)
{
	int i;
	struct audio_backend *be = (struct audio_backend *)wb;

	for (i = 0; i < delay; i++) {
		int j;
		uint8_t val = 0;
		for (j = 0; j < be->n_channels; j++) {
			if (be->state & (1 << j)) {
				val += (1 << 5) - 1;
			}
		}
		fwrite(&val, 1, 1, stdout);
	}

	be->time += delay;
}

void audio_backend_fini(struct audio_backend *be)
{
	free(be);
}

struct audio_backend *audio_backend_create(uint32_t pins)
{
	struct audio_backend *be = calloc(1, sizeof(*be));
	int i, n;

	be->base.add_delay = audio_backend_add_delay;
	be->base.add_event = audio_backend_add_event;

	for (i = 0; i < 32; i++) {
		if (pins & (1 << i)) {
			be->n_channels++;
		}
	}

	return be;
}

struct platform {
	struct audio_backend *be;
};

void platform_fini(struct platform *p)
{
	if (p->be) {
		audio_backend_fini(p->be);
	}
	free(p);
}

struct platform *platform_init(uint32_t pins)
{
	struct platform *p = calloc(1, sizeof(*p));
	if (!p) {
		return NULL;
	}

	p->be = audio_backend_create(pins);
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
	usleep(16000);
	return 0;
}

void platform_dump(struct platform *p)
{
	return;
}
