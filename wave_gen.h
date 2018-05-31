/*
 * wave_gen.h Event sequence generator
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
#ifndef __WAVE_GEN_H__
#define __WAVE_GEN_H__

#define MAX_SOURCES 4

/* event is defined by the backend */
struct event;

struct source {
	int (*get_delay)(struct source *);
	void (*gen_event)(struct source *, struct event *ev);
};

struct wave_backend {
	void (*add_delay)(struct wave_backend *wb, int delay);
	void (*add_event)(struct wave_backend *wb, struct source *s);
};

struct wave_ctx {
	int n_sources;
	struct wave_backend *be;
	struct source *sources[MAX_SOURCES];

	int t[MAX_SOURCES];
};

void wave_gen(struct wave_ctx *c, int budget);

#endif /* __WAVE_GEN_H__ */
