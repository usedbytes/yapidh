/*
 * step_source.h Event source for stepper motor
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
#ifndef __STEP_SOURCE_H__
#define __STEP_SOURCE_H__

#include "wave_gen.h"
#include "step_gen.h"

enum edge {
	EDGE_RISING,
	EDGE_FALLING,
};

struct step_source {
	struct source base;
	struct step_ctx sctx;

	int edge;
	int gap;
	int pulsewidth;
	int channel;
};

struct step_source *step_source_create(int channel);
void step_source_set_speed(struct source *s, double speed);

#endif /* __STEP_SOURCE_H__ */
