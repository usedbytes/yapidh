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
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "step_source.h"
#include "step_gen.h"
#include "wave_gen.h"

#include "platform.h"

volatile bool exiting = false;
static void sig_handler(int dummy)
{
	exiting = true;
}

static void setup_sighandlers(void)
{
	int i;

	// Catch all signals possible - it is vital we kill the DMA engine
	// on process exit!
	for (i = 0; i < 64; i++) {
		struct sigaction sa;

		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = sig_handler;
		sigaction(i, &sa, NULL);
	}
}

static int random_number(int min, int max) {
	int range = max + 1 - min;
	return (rand() % range) + min;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	struct wave_ctx ctx = {
		.n_sources = 4,
		.sources = {
			(struct source *)step_source_create(16),
			(struct source *)step_source_create(19),
			(struct source *)step_source_create(20),
			(struct source *)step_source_create(21),
		},
	};
	uint32_t pins = (1 << 16) | (1 << 19) | (1 << 20) | (1 << 21);

	struct platform *p = platform_init(pins);
	if (!p) {
		fprintf(stderr, "Platform creation failed\n");
		return 1;
	}

	setup_sighandlers();

	ctx.be = platform_get_backend(p);

	srand(1024);

	int next_change[] = { 0, 0, 0, 0};
	int speed[] = {
		random_number(1, 24),
		random_number(1, 24),
		random_number(1, 24),
		random_number(1, 24),
	};
	int i;


	while (!exiting) {
		ret = platform_sync(p, 1000);
		if (ret) {
			fprintf(stderr, "Timeout waiting for fence\n");
			goto fail;
		}

		wave_gen(&ctx, 1600);

		for (i = 0; i < ctx.n_sources; i++) {
			if (next_change[i] > 0) {
				next_change[i]--;
			} else if (next_change[i] == 0) {
				step_source_set_speed(ctx.sources[i], random_number(1, 24));
				next_change[i] = random_number(0, 60);
			}
		}
	}

fail:
	platform_fini(p);
	return ret;
}
