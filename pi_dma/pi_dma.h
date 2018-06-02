/*
 * Copyright (c) 2018 Brian Starkey <stark3y@gmail.com>
 * Portions derived from servod.
 * Copyright (c) 2013 Richard Hirst <richardghirst@gmail.com>
 *
 * This program provides very similar functionality to servoblaster, except
 * that rather than implementing it as a kernel module, servod implements
 * the functionality as a usr space daemon.
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
#ifndef __PI_DMA_H__
#define __PI_DMA_H__
#include <stdbool.h>
#include <stdint.h>

#include "pi_util.h"

struct dma_channel;

typedef struct {
	uint32_t info;
	uint32_t src;
	uint32_t dst;
	uint32_t length;
	uint32_t stride;
	uint32_t next;
	uint32_t pad[2];
} dma_cb_t;

enum dma_pacer {
	PACER_NONE = -1,
	PACER_PWM,
	PACER_PCM,
};

struct dma_channel *dma_channel_init(struct board_cfg *board, int channel);
void dma_channel_fini(struct dma_channel *ch);

void dma_channel_setup_pacer(struct dma_channel *ch, enum dma_pacer pacer,
			     uint32_t pace_us);
void dma_channel_run(struct dma_channel *ch, uint32_t cb_base_phys);

void dma_rising_edge(struct dma_channel *ch, uint32_t pins, dma_cb_t *cb, uint32_t cb_phys);
void dma_falling_edge(struct dma_channel *ch, uint32_t pins, dma_cb_t *cb, uint32_t cb_phys);
int dma_delay(struct dma_channel *ch, uint32_t delay_us, dma_cb_t *cb, uint32_t cb_phys);
void dma_fence(struct dma_channel *ch, uint32_t val, dma_cb_t *cb, uint32_t cb_phys);
int dma_fence_wait(dma_cb_t *cb, int timeout_millis, int sleep_millis);
bool dma_fence_signaled(dma_cb_t *cb);

void dma_channel_dump(struct dma_channel *ch);
void dma_cb_dump(dma_cb_t *cb);

#endif /* __PI_DMA_H__ */
