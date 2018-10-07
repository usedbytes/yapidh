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
#include <errno.h>
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

struct step_report {
	uint32_t motor;
	int32_t steps;
};

int main(int argc, char *argv[])
{
	int i, n = 0, ret = 0;
	struct wave_ctx ctx = {
		.n_sources = 4,
		.sources = {
			stepper_create(12,6,8),
			stepper_create(5,7,11),
			stepper_create(26,20,16),
			stepper_create(19,13,21),
		},
	};
	uint32_t pins = (1 << 21) | (1 << 20) | (1 << 16) |
			(1 << 26) | (1 << 19) | (1 << 13) |
			(1 << 12) | (1 << 6) | (1 << 5) |
			(1 << 11) | (1 << 8) | (1 << 7);

	struct platform *p = platform_init(pins);
	if (!p) {
		fprintf(stderr, "Platform creation failed\n");
		return 1;
	}

	struct comm *comm = comm_init_unix("/tmp/sock");
	if (!comm) {
		fprintf(stderr, "Comm creation failed\n");
		goto fail;
	}
	struct comm_packet *pkts;

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

	int last1 = 0, last2 = 0;

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
			for (i = 0; i < ret; i++) {
				struct comm_packet *p = &pkts[i];
				switch (p->type) {
					/* Set Speed */
					case 1: {
						stepper_set_velocity(ctx.sources[p->data[0]], (double)((int8_t)p->data[1]));
						stepper_set_velocity(ctx.sources[p->data[0] + 2], (double)((int8_t)(p->data[1])));
					}
				}
			}
			comm_free_packets(pkts, ret);
		} else if (ret < 0 && ret != -EAGAIN) {
			fprintf(stderr, "comm_poll failed\n");
		}

		struct step_report rep;

		rep.motor = 0,
		rep.steps = stepper_get_steps(ctx.sources[0]);
		if (rep.steps || last1 != 0) {
			comm_send(comm, 0x12, sizeof(rep), (uint8_t *)&rep);
		}
		last1 = rep.steps;

		rep.motor = 1,
		rep.steps = stepper_get_steps(ctx.sources[1]);
		if (rep.steps || last2 != 0) {
			comm_send(comm, 0x12, sizeof(rep), (uint8_t *)&rep);
		}
		last2 = rep.steps;
	}
#endif

fail:
	platform_fini(p);
	return ret;
}
