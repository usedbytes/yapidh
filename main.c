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

#include "stepper_driver.h"
#include "comm.h"
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

int main(int argc, char *argv[])
{
	int i, ret = 0;
	struct wave_ctx ctx = {
		.n_sources = 4,
		.sources = {
			stepper_create(0, 1, 2),
			stepper_create(3, 4, 5),
			stepper_create(6, 7, 8),
			stepper_create(9, 10, 11),
		},
	};
	uint32_t pins = (1 << 12) - 1; //(1 << 16); // | (1 << 19) | (1 << 20) | (1 << 21);

	struct platform *p = platform_init(pins);
	if (!p) {
		fprintf(stderr, "Platform creation failed\n");
		return 1;
	}

	struct comm *comm = comm_init("/tmp/sock");
	if (!comm) {
		fprintf(stderr, "Comm creation failed\n");
		goto fail;
	}
	struct comm_packet **pkts;

	setup_sighandlers();

#if 0
	int i;
	int speed[] = { 10, -15, 0, 6, 8, -1 };
	for (i = 0; i < sizeof(speed) / sizeof(speed[0]); i++) {
		stepper_set_velocity(ctx.sources[0], speed[i]);
		wave_gen(&ctx, 50000);
	}

	return 0;

#else

	ctx.be = platform_get_backend(p);

	while (!exiting) {
		ret = platform_sync(p, 1000);
		if (ret) {
			fprintf(stderr, "Timeout waiting for fence\n");
			platform_dump(p);
			goto fail;
		}

		wave_gen(&ctx, 1600);

		ret = comm_poll(comm, &pkts);
		if (ret > 0) {
			fprintf(stderr, "Received %d packets\n", ret);
			for (i = 0; i < ret; i++) {
				struct comm_packet *p = pkts[i];
				switch (p->type) {
					/* Set Speed */
					case 1: {
						fprintf(stderr, "set speed %d %f\n", p->data[0], (double)((int8_t)p->data[1]));
						stepper_set_velocity(ctx.sources[p->data[0]], (double)((int8_t)p->data[1]));
					}
				}
				free(pkts[i]);
			}
			free(pkts);
		} else if (ret < 0) {
			fprintf(stderr, "comm_poll failed\n");
		}
	}
#endif

fail:
	platform_fini(p);
	return ret;
}
