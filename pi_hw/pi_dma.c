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
#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include "pi_dma.h"
#include "pi_clk.h"

#define GPIO_BASE_OFFSET	0x00200000
#define DMA_BASE_OFFSET		0x00007000
#define DMA_CHAN_SIZE		0x100
#define DMA_CHAN_MIN		0
#define DMA_CHAN_MAX		14

#define DMA_NO_WIDE_BURSTS	(1<<26)
#define DMA_WAIT_RESP		(1<<3)
#define DMA_D_DREQ		(1<<6)
#define DMA_PER_MAP(x)		((x)<<16)
#define DMA_END			(1<<1)
#define DMA_RESET		(1<<31)
#define DMA_INT			(1<<2)
#define DMA_SRC_IGNORE		(1<<11)
#define DMA_TDMODE		(1<<1)

#define DMA_CS			(0x00/4)
#define DMA_CONBLK_AD		(0x04/4)
#define DMA_SOURCE_AD		(0x0c/4)
#define DMA_DEBUG		(0x20/4)

#define PWM_BASE_OFFSET		0x0020C000
#define PWM_LEN			0x28
#define PCM_BASE_OFFSET		0x00203000
#define PCM_LEN			0x24

#define PWM_CTL			(0x00/4)
#define PWM_DMAC		(0x08/4)
#define PWM_RNG1		(0x10/4)
#define PWM_FIFO		(0x18/4)
#define PWMCTL_MODE1		(1<<1)
#define PWMCTL_PWEN1		(1<<0)
#define PWMCTL_CLRF		(1<<6)
#define PWMCTL_USEF1		(1<<5)

#define PWMDMAC_ENAB		(1<<31)
#define PWMDMAC_THRSHLD		((15<<8)|(15<<0))

#define PCM_CS_A		(0x00/4)
#define PCM_FIFO_A		(0x04/4)
#define PCM_MODE_A		(0x08/4)
#define PCM_RXC_A		(0x0c/4)
#define PCM_TXC_A		(0x10/4)
#define PCM_DREQ_A		(0x14/4)
#define PCM_INTEN_A		(0x18/4)
#define PCM_INT_STC_A		(0x1c/4)
#define PCM_GRAY		(0x20/4)

#define PCMCLK_CNTL		38
#define PCMCLK_DIV		39

struct dma_channel {
	uint32_t *map;
	size_t maplen;
	uint32_t *reg;

	enum dma_pacer pacer;
	uint32_t pace_us;

	uint32_t periph_phys_base;
};

struct clock_dev *clk_dev;
uint32_t *pwm_reg;
uint32_t *pcm_reg;

struct dma_channel *dma_channel_init(struct board_cfg *board, int channel)
{
	struct dma_channel *ch;

	/* FIXME: These globals are a bit ugly */
	if (!clk_dev) {
		clk_dev = clock_init(board);
		if (!clk_dev) {
			return NULL;
		}
	}

	if (!pwm_reg) {
		pwm_reg = map_peripheral(board->periph_virt_base + PWM_BASE_OFFSET,
					 PWM_LEN);
		if (pwm_reg == MAP_FAILED) {
			return NULL;
		}
	}

	if (!pcm_reg) {
		pcm_reg = map_peripheral(board->periph_virt_base + PCM_BASE_OFFSET,
					 PCM_LEN);
		if (pcm_reg == MAP_FAILED) {
			return NULL;
		}
	}

	if ((channel < DMA_CHAN_MIN) || (channel > DMA_CHAN_MAX)) {
		return NULL;
	}

	ch = calloc(1, sizeof(*ch));
	if (!ch) {
		return NULL;
	}

	ch->periph_phys_base = board->periph_phys_base;
	ch->pacer = PACER_NONE;
	ch->maplen = DMA_CHAN_SIZE * (channel + 1);
	ch->map = map_peripheral(board->periph_virt_base + DMA_BASE_OFFSET,
				  ch->maplen);
	if (ch->map == MAP_FAILED) {
		goto fail;
	}
	ch->reg = ch->map + ((DMA_CHAN_SIZE * channel) / 4);

	return ch;

fail:
	dma_channel_fini(ch);
	return NULL;
}

void dma_channel_fini(struct dma_channel *ch)
{
	if (ch->map != MAP_FAILED) {
		dma_channel_setup_pacer(ch, PACER_NONE, 0);
		ch->reg[DMA_CS] = DMA_RESET;
		usleep(10);
		munmap(ch->map, ch->maplen);
	}
	free(ch);
}


void dma_channel_setup_pacer(struct dma_channel *ch, enum dma_pacer pacer,
			     uint32_t pace_us)
{
	switch (pacer) {
	case PACER_PWM:
		// Initialise PWM
		pwm_reg[PWM_CTL] = 0;
		usleep(10);
		clock_set_rate(clk_dev, CLOCK_CONSUMER_PWM, 1000000);
		pwm_reg[PWM_RNG1] = pace_us;
		usleep(10);
		pwm_reg[PWM_DMAC] = PWMDMAC_ENAB | PWMDMAC_THRSHLD;
		usleep(10);
		pwm_reg[PWM_CTL] = PWMCTL_CLRF;
		usleep(10);
		pwm_reg[PWM_CTL] = PWMCTL_USEF1 | PWMCTL_PWEN1;
		usleep(10);
		break;
	case PACER_PCM:
		// Initialise PCM
		pcm_reg[PCM_CS_A] = 1;				// Disable Rx+Tx, Enable PCM block
		usleep(100);
		clock_set_rate(clk_dev, PACER_PCM, 1000000);
		pcm_reg[PCM_TXC_A] = 0<<31 | 1<<30 | 0<<20 | 0<<16; // 1 channel, 8 bits
		usleep(100);
		pcm_reg[PCM_MODE_A] = (pace_us - 1) << 10;
		usleep(100);
		pcm_reg[PCM_CS_A] |= 1<<4 | 1<<3;		// Clear FIFOs
		usleep(100);
		pcm_reg[PCM_DREQ_A] = 64<<24 | 64<<8;		// DMA Req when one slot is free?
		usleep(100);
		pcm_reg[PCM_CS_A] |= 1<<9;			// Enable DMA
		usleep(100);
		break;
	default:
		if (ch->pacer == PACER_PWM) {
			pwm_reg[PWM_CTL] = 0;
			usleep(10);
			pwm_reg[PWM_DMAC] = 0;
		} else if (ch->pacer == PACER_PCM) {
			pcm_reg[PCM_CS_A] = 1;				// Disable Rx+Tx, Enable PCM block
			usleep(100);
			pcm_reg[PCM_CS_A] = ~(1<<9);			// Disable DMA
		}
		break;
	}
	ch->pacer = pacer;
	ch->pace_us = pace_us;

	return;
}

void dma_channel_run(struct dma_channel *ch, uint32_t cb_dma_addr)
{
	// Initialise the DMA
	ch->reg[DMA_CS] = DMA_RESET;
	/* TODO: If this sleep is really needed, then we need a "fast" warm
	 * path which doesn't do the reset (in case of underruns)
	 * Or is underrun so catastrophic that we don't care?
	 */
	usleep(10);
	ch->reg[DMA_CS] = DMA_INT | DMA_END;
	ch->reg[DMA_CONBLK_AD] = cb_dma_addr;
	ch->reg[DMA_DEBUG] = 7; // clear debug error flags
	ch->reg[DMA_CS] = 0x10880001;	// go, mid priority, wait for outstanding writes

	if (ch->pacer == PACER_PCM) {
		pcm_reg[PCM_CS_A] |= 1<<2;			// Enable Tx
	}
}

/* TODO: Do we need access to pins 32-53 ? */
void dma_rising_edge(struct dma_channel *ch, uint32_t pins, dma_cb_t *cb, uint32_t cb_dma_addr)
{
	cb->info = DMA_NO_WIDE_BURSTS | DMA_WAIT_RESP;
	cb->src = cb_dma_addr + offsetof(dma_cb_t, pad);
	cb->dst = ch->periph_phys_base + GPIO_BASE_OFFSET + 0x1c;
	cb->length = 4;
	cb->stride = 0;
	cb->next = (uint32_t)NULL;
	cb->pad[0] = pins;
}

void dma_falling_edge(struct dma_channel *ch, uint32_t pins, dma_cb_t *cb, uint32_t cb_dma_addr)
{
	cb->info = DMA_NO_WIDE_BURSTS | DMA_WAIT_RESP;
	cb->src = cb_dma_addr + offsetof(dma_cb_t, pad);
	cb->dst = ch->periph_phys_base + GPIO_BASE_OFFSET + 0x28;
	cb->length = 4;
	cb->stride = 0;
	cb->next = (uint32_t)NULL;
	cb->pad[0] = pins;
}

int dma_delay(struct dma_channel *ch, uint32_t delay_us, dma_cb_t *cb, uint32_t cb_dma_addr)
{
	uint32_t phys_fifo_addr;
	if (ch->pacer == PACER_NONE || !ch->pace_us) {
		return -1;
	}

	if (delay_us % ch->pace_us) {
		return -1;
	}

	if (ch->pacer == PACER_PWM) {
		phys_fifo_addr = (ch->periph_phys_base + PWM_BASE_OFFSET) + 0x18;
		cb->info = DMA_NO_WIDE_BURSTS | DMA_WAIT_RESP | DMA_D_DREQ | DMA_PER_MAP(5) | DMA_SRC_IGNORE | DMA_TDMODE;
	} else if (ch->pacer == PACER_PCM) {
		phys_fifo_addr = (ch->periph_phys_base + PCM_BASE_OFFSET) + 0x04;
		cb->info = DMA_NO_WIDE_BURSTS | DMA_WAIT_RESP | DMA_D_DREQ | DMA_PER_MAP(2) | DMA_SRC_IGNORE | DMA_TDMODE;
	}

	delay_us /= ch->pace_us;

	cb->src = cb_dma_addr + offsetof(dma_cb_t, pad);
	cb->dst = phys_fifo_addr;
	cb->length = ((delay_us - 1) << 16) | 4;
	cb->stride = 0;
	cb->next = (uint32_t)NULL;

	return 0;
}

void dma_fence(struct dma_channel *ch, uint32_t val, dma_cb_t *cb, uint32_t cb_dma_addr)
{
	cb->info = DMA_NO_WIDE_BURSTS | DMA_WAIT_RESP;
	cb->src = cb_dma_addr + offsetof(dma_cb_t, pad);
	cb->dst = cb_dma_addr + offsetof(dma_cb_t, pad) + 4;
	cb->length = 4;
	cb->stride = 0;
	cb->next = (uint32_t)NULL;
	cb->pad[0] = val;
	cb->pad[1] = 0;
}

bool dma_fence_signaled(dma_cb_t *cb)
{
	volatile uint32_t *f = (volatile uint32_t *)&cb->pad[1];
	return *f;
}

int dma_fence_wait(dma_cb_t *cb, int timeout_millis, int sleep_millis)
{
	int us = sleep_millis * 1000;
	while (1) {
		if (dma_fence_signaled(cb)) {
			return 0;
		}

		if (timeout_millis > 0) {
			timeout_millis--;
		} else if (timeout_millis == 0) {
			return -1;
		}
		usleep(us);
	}
}

void dma_channel_dump(struct dma_channel *ch)
{
	printf("CS: %08x\n", ch->reg[0]);
	printf("CAD: %08x\n", ch->reg[1]);
	printf("TI: %08x\n", ch->reg[2]);
	printf("SAD: %08x\n", ch->reg[3]);
	printf("DAD: %08x\n", ch->reg[4]);
	printf("LEN: %08x\n", ch->reg[5]);
	printf("STR: %08x\n", ch->reg[6]);
	printf("NXT: %08x\n", ch->reg[7]);
	printf("DBG: %08x\n", ch->reg[8]);
}

void dma_cb_dump(dma_cb_t *cb)
{
	printf("VA : %08x\n", (uint32_t)cb);
	printf("TI : %08x\n", cb->info);
	printf("SAD: %08x\n", cb->src);
	printf("DAD: %08x\n", cb->dst);
	printf("LEN: %08x\n", cb->length);
	printf("STR: %08x\n", cb->stride);
	printf("NXT: %08x\n", cb->next);
	printf("PA0: %08x\n", cb->pad[0]);
	printf("PA1: %08x\n", cb->pad[1]);
}
