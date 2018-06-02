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
#include "vcd_backend.h"

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

static dma_cb_t *square_wave(struct dma_channel *ch, struct phys *phys, dma_cb_t *cb,
			     uint32_t period_us, uint32_t cycles)
{
	int i;

	for (i = 0; i < cycles; i++) {
		dma_rising_edge(ch, (1 << 4), cb, phys_virt_to_phys(phys, cb));
		cb->next = phys_virt_to_phys(phys, cb + 1);
		cb++;

		dma_delay(ch, period_us / 2, cb, phys_virt_to_phys(phys, cb));
		cb->next = phys_virt_to_phys(phys, cb + 1);
		cb++;

		dma_falling_edge(ch, (1 << 4), cb, phys_virt_to_phys(phys, cb));
		cb->next = phys_virt_to_phys(phys, cb + 1);
		cb++;

		dma_delay(ch, period_us / 2, cb, phys_virt_to_phys(phys, cb));
		cb->next = phys_virt_to_phys(phys, cb + 1);
		cb++;
	}

	cb--;
	return cb;
}

int main(int argc, char *argv[])
{
	int ret;
	struct board_cfg board;
	struct gpio_dev *gpio = NULL;
	struct phys *phys = NULL;
	struct dma_channel *dma_ch = NULL;
	dma_cb_t *cb, *cursor, *tail, *head, *fence;
	dma_cb_t *wave0, *wave1;
	int wave_idx = 0;
	unsigned period = 200;

	setup_sighandlers();

	ret = get_model_and_revision(&board);
	if (ret < 0) {
		return 1;
	}

	printf("Board:\n");
	printf("Model: %d\n", board.board_model);
	printf("GPIO_Cfg: %d\n", board.gpio_cfg);
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

	phys = phys_alloc(&board, sizeof(dma_cb_t) * 2048);
	if (!phys) {
		fprintf(stderr, "Couldn't get phys\n");
		goto fail;
	}

	printf("Phys handle: %d\n", phys->handle);
	printf("Phys size: %d\n", phys->size);
	printf("Phys ref: %d\n", phys->mem_ref);
	printf("Phys bus: %08x\n", phys->bus_addr);
	printf("Phys virt: %p\n", phys->virt_addr);

	dma_ch = dma_channel_init(&board, 6);
	if (!dma_ch) {
		fprintf(stderr, "Couldn't get dma\n");
		goto fail;
	}

	dma_channel_setup_pacer(dma_ch, PACER_PWM, 10);

	cb = (dma_cb_t *)phys->virt_addr;

	wave0 = cb;
	wave1 = cb + 1024;

	cursor = wave0;

	// Insert a fence
	dma_fence(dma_ch, 1, cursor, phys_virt_to_phys(phys, cursor));
	fence = cursor;
	cursor->next = phys_virt_to_phys(phys, cursor + 1);
	cursor++;

	// Generate 16ms of wave
	cursor = square_wave(dma_ch, phys, cursor, period, 16000 / period);
	cursor->next = 0;

	tail = cursor;

	wave_idx = 1;

	dma_cb_dump(fence);
	dma_channel_run(dma_ch, phys_virt_to_phys(phys, wave0));

	while (!exiting) {
#if 1
		ret = dma_fence_wait(fence, 10, 5);
		if (ret < 0) {
			fprintf(stderr, "Timeout waiting for fence.\n");
			goto fail;
		};

		if (wave_idx == 1) {
			head = cursor = wave1;
			wave_idx = 0;
			period = 800;
		} else {
			head = cursor = wave0;
			wave_idx = 1;
			period = 100;
		}

		// Insert a fence
		dma_fence(dma_ch, 1, cursor, phys_virt_to_phys(phys, cursor));
		fence = cursor;
		cursor->next = phys_virt_to_phys(phys, cursor + 1);
		cursor++;

		// Generate 16ms of wave
		cursor = square_wave(dma_ch, phys, cursor, period, 16000 / period);
		cursor->next = 0;

		tail->next = phys_virt_to_phys(phys, head);
		tail = cursor;
#else
		usleep(1000000);
		dma_cb_dump(fence);
#endif
	}

	/*
	int i;
	struct step_source *ss = step_source_create();
	struct step_source *ss2 = step_source_create();
	const char *names[] = {
		"ch0", "ch1", "ch2", "ch3",
	};
	struct vcd_backend *be = vcd_backend_create(4, names);

	struct wave_ctx ctx = {
		.n_sources = 2,
		.sources = { &ss->base, &ss2->base },
		.be = &be->base,
	};

	stepper_set_speed(&ss2->sctx, 7);
	stepper_set_speed(&ss->sctx, 24);

	for (i = 0; i < 60; i++) {
		wave_gen(&ctx, 1600);
	}
	*/

fail:
	if (dma_ch) {
		dma_channel_fini(dma_ch);
	}

	if (phys) {
		phys_free(phys);
	}

	return 0;
}
