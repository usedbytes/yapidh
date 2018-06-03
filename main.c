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
#include <string.h>
#include <unistd.h>

#include "step_source.h"
#include "step_gen.h"
#include "wave_gen.h"
#include "pi_backend.h"

#include "pi_hw/pi_dma.h"
#include "pi_hw/pi_gpio.h"
#include "pi_hw/pi_util.h"

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
	int ret;
	struct board_cfg board;
	struct gpio_dev *gpio = NULL;
	struct pi_backend *be = NULL;
	struct step_source *ss = step_source_create();
	struct wave_ctx ctx = {
		.n_sources = 1,
		.sources = { &ss->base },
	};

	setup_sighandlers();

	ret = get_model_and_revision(&board);
	if (ret < 0) {
		return 1;
	}

	printf("Board:\n");
	printf("Periph phys: %08x\n", board.periph_phys_base);
	printf("Periph virt: %08x\n", board.periph_virt_base);
	printf("Dram phys: %08x\n", board.dram_phys_base);
	printf("mem_flag: %08x\n", board.mem_flag);


	gpio = gpio_init(&board);
	if (!gpio) {
		fprintf(stderr, "Couldn't get GPIO\n");
		goto fail;
	}

	gpio_set_mode(gpio, 4, GPIO_MODE_OUT);
	gpio_clear(gpio, (1 << 4));

#ifdef DEBUG
	gpio_set_mode(gpio, DBG_CHUNK_PIN, GPIO_MODE_OUT);
	gpio_clear(gpio, (1 << DBG_CHUNK_PIN));

	gpio_set_mode(gpio, DBG_CPUTIME_PIN, GPIO_MODE_OUT);
	gpio_clear(gpio, (1 << DBG_CPUTIME_PIN));

	gpio_set_mode(gpio, DBG_FENCE_PIN, GPIO_MODE_OUT);
	gpio_clear(gpio, (1 << DBG_FENCE_PIN));

	sleep(1);
#endif

	be = pi_backend_create(&board);
	if (!be) {
		fprintf(stderr, "Couldn't get backend\n");
		goto fail;
	}

	ctx.be = (struct wave_backend *)be;

	stepper_set_speed(&ss->sctx, 24);

	while (!exiting) {
		gpio_debug_set(gpio, 1 << DBG_FENCE_PIN);
		ret = pi_backend_wait_fence(be);
		if (ret < 0) {
			fprintf(stderr, "Timeout waiting for fence.\n");
			goto fail;
		};
		gpio_debug_clear(gpio, 1 << DBG_FENCE_PIN);

		gpio_debug_set(gpio, 1 << DBG_CPUTIME_PIN);
		pi_backend_wave_start(be);
		wave_gen(&ctx, 1600);
		pi_backend_wave_end(be);
		gpio_debug_clear(gpio, 1 << DBG_CPUTIME_PIN);
	}

fail:
	if (gpio) {
		gpio_fini(gpio);
	}
	if (be) {
		pi_backend_destroy(be);
	}

	return 0;
}
