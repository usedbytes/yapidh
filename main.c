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

#include "step_source.h"
#include "step_gen.h"
#include "wave_gen.h"

int main(int argc, char *argv[])
{
	struct step_source *ss = step_source_create();

	struct wave_ctx ctx = {
		.n_sources = 1,
		.sources = { &ss->base },
	};

	stepper_set_speed(&ss->sctx, 24);

	wave_gen(&ctx, 1000000);

	return 0;
}
