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

#include "step_source.h"

static int step_source_get_delay(struct source *s)
{
	struct step_source *ss = (struct step_source *)s;

	stepper_tick(&ss->sctx);

	return round(ss->sctx.c);
}

static void step_source_gen_event(struct source *s)
{
	struct step_source *ss = (struct step_source *)s;

	// TODO
	step_ctx_dump(&ss->sctx);
}

struct step_source *step_source_create()
{
	struct step_source *ss = calloc(1, sizeof(*ss));

	ss->base.gen_event = step_source_gen_event;
	ss->base.get_delay = step_source_get_delay;

	step_ctx_init(&ss->sctx, 600, 100000, 100);

	return ss;
}
