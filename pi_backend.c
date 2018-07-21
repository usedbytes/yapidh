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

#include "pi_backend.h"
#include "pi_hw/pi_dma.h"
#include "pi_hw/pi_gpio.h"
#include "pi_hw/pi_util.h"
#include "types.h"
#include "wave_gen.h"

#define N_CBS (4096)
#define DMA_TICK_US 10

struct pi_backend {
	struct wave_backend base;
	struct dma_channel *dma;
	struct phys *phys;

	uint32_t rising;
	uint32_t falling;

	int wave_idx;
	dma_cb_t *waves[2];
	dma_cb_t *tail;
	dma_cb_t *fence;
	dma_cb_t *cursor;

	// Debug
	dma_cb_t *prev_tail;

	// Not owned by us. (and debug only... FIXME: this is ugly)
	struct gpio_dev *gpio;
};


static uint32_t pi_backend_add_event(struct wave_backend *wb, struct source *s)
{
	struct pi_backend *be = (struct pi_backend *)wb;
	struct event ev = { 0 };
	uint32_t ret;

	ret = s->gen_event(s, &ev);

	be->rising |= ev.rising;
	be->falling |= ev.falling;

	return ret;
}

static void pi_backend_add_delay(struct wave_backend *wb, int delay)
{
	struct pi_backend *be = (struct pi_backend *)wb;
	dma_cb_t *cb = be->cursor;

	dma_rising_edge(be->dma, be->rising, cb, phys_virt_to_bus(be->phys, cb));
	cb->next = phys_virt_to_bus(be->phys, cb + 1);
	cb++;

	dma_falling_edge(be->dma, be->falling, cb, phys_virt_to_bus(be->phys, cb));
	cb->next = phys_virt_to_bus(be->phys, cb + 1);
	cb++;

	dma_delay(be->dma, delay * DMA_TICK_US, cb, phys_virt_to_bus(be->phys, cb));
	cb->next = phys_virt_to_bus(be->phys, cb + 1);
	cb++;

	be->cursor = cb;
	be->rising = be->falling = 0;
}

static void pi_backend_start_wave(struct wave_backend *wb)
{
	struct pi_backend *be = (struct pi_backend *)wb;

	gpio_debug_set(be->gpio, 1 << DBG_CPUTIME_PIN);

	be->cursor = be->waves[be->wave_idx];

	// Insert a fence
	dma_fence(be->dma, 1, be->cursor, phys_virt_to_bus(be->phys, be->cursor));
	be->fence = be->cursor;
	be->cursor->next = phys_virt_to_bus(be->phys, be->cursor + 1);
	be->cursor++;

#ifdef DEBUG
	/* Mark chunks for debugging */
	if (be->wave_idx) {
		dma_rising_edge(be->dma, (1 << DBG_CHUNK_PIN), be->cursor, phys_virt_to_bus(be->phys, be->cursor));
	} else {
		dma_falling_edge(be->dma, (1 << DBG_CHUNK_PIN), be->cursor, phys_virt_to_bus(be->phys, be->cursor));
	}
	be->cursor->next = phys_virt_to_bus(be->phys, be->cursor + 1);
	be->cursor++;
#endif
}

static void pi_backend_end_wave(struct wave_backend *wb)
{
	struct pi_backend *be = (struct pi_backend *)wb;
	uint32_t n_cbs;

#ifdef DEBUG
	/* Mark chunks for debugging */
	if (be->wave_idx) {
		dma_falling_edge(be->dma, (1 << DBG_CHUNK_PIN), be->cursor, phys_virt_to_bus(be->phys, be->cursor));
	} else {
		dma_rising_edge(be->dma, (1 << DBG_CHUNK_PIN), be->cursor, phys_virt_to_bus(be->phys, be->cursor));
	}
	be->cursor->next = phys_virt_to_bus(be->phys, be->cursor + 1);
	be->cursor++;
#endif

	// Insert a dummy transaction - if the last "real" element is a long
	// delay, then it could get loaded (and so the "->next" pointer frozen)
	// before we set up the next segment.
	dma_fence(be->dma, 1, be->cursor, phys_virt_to_bus(be->phys, be->cursor));
	be->cursor->next = (uint32_t)NULL;

	be->tail->next = phys_virt_to_bus(be->phys, be->waves[be->wave_idx]);
	be->tail = be->cursor;

	n_cbs = be->cursor - be->waves[be->wave_idx];
	if (n_cbs > (N_CBS / 4)) {
		fprintf(stderr, "Used %d (of %d) CBs for this wave\n", n_cbs, N_CBS / 2);
	}
	be->cursor = NULL;
	be->wave_idx = !be->wave_idx;

	gpio_debug_clear(be->gpio, 1 << DBG_CPUTIME_PIN);
}

struct pi_backend *pi_backend_create(struct board_cfg *board, struct gpio_dev *gpio)
{
	uint32_t cb_dma_addr;
	struct pi_backend *be = calloc(1, sizeof(*be));
	if (!be) {
		return NULL;
	}

	be->base.start_wave = pi_backend_start_wave;
	be->base.add_delay = pi_backend_add_delay;
	be->base.add_event = pi_backend_add_event;
	be->base.end_wave = pi_backend_end_wave;

	be->gpio = gpio;

	be->phys = phys_alloc(board, sizeof(dma_cb_t) * N_CBS);
	if (!be->phys) {
		fprintf(stderr, "Couldn't get phys\n");
		goto fail;
	}

	be->dma = dma_channel_init(board, 6);
	if (!be->dma) {
		fprintf(stderr, "Couldn't get dma\n");
		goto fail;
	}
	dma_channel_setup_pacer(be->dma, PACER_PWM, DMA_TICK_US);

	be->waves[0] = (dma_cb_t *)be->phys->virt_addr;
	be->waves[1] = &be->waves[0][N_CBS / 2];

	/*
	 * Start the DMA off looping in wave 1. Fence included only for
	 * consistency
	 */
	cb_dma_addr = phys_virt_to_bus(be->phys, &be->waves[be->wave_idx][0]);
	dma_fence(be->dma, 1, &be->waves[be->wave_idx][0], cb_dma_addr);
	be->fence = &be->waves[be->wave_idx][0];
	be->waves[be->wave_idx][0].next = cb_dma_addr + sizeof(dma_cb_t);

	dma_delay(be->dma, 8000, be->waves[be->wave_idx] + 1, cb_dma_addr + sizeof(dma_cb_t));
	be->waves[be->wave_idx][1].next = cb_dma_addr;
	be->prev_tail = be->tail;
	be->tail = &be->waves[be->wave_idx][1];

	be->wave_idx = !be->wave_idx;

	dma_channel_run(be->dma, cb_dma_addr);

	return be;

fail:
	pi_backend_destroy(be);
	return NULL;
}

void pi_backend_destroy(struct pi_backend *be)
{
	if (be->dma) {
		dma_channel_fini(be->dma);
	}
	if (be->phys) {
		phys_free(be->phys);
	}
	free(be);
}

int pi_backend_wait_fence(struct pi_backend *be, int timeout_millis,
			  int sleep_millis)
{
	return dma_fence_wait(be->fence, timeout_millis, sleep_millis);
}

void pi_backend_dump(struct pi_backend *be)
{
	fprintf(stderr, "waves[0]: %p\n", be->waves[0]);
	dma_cb_dump(be->waves[0]);
	fprintf(stderr, "---\n");

	fprintf(stderr, "waves[1]: %p\n", be->waves[1]);
	dma_cb_dump(be->waves[1]);
	fprintf(stderr, "---\n");

	fprintf(stderr, "Prev Tail:\n");
	dma_cb_dump(be->prev_tail);
	fprintf(stderr, "---\n");

	fprintf(stderr, "Fence:\n");
	dma_cb_dump(be->fence);
	fprintf(stderr, "---\n");

	fprintf(stderr, "Tail:\n");
	dma_cb_dump(be->tail);
	fprintf(stderr, "---\n");

	fprintf(stderr, "Dma chan\n");
	dma_channel_dump(be->dma);
	fprintf(stderr, "---\n");
}
