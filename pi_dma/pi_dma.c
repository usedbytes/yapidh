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
#include <sys/mman.h>
#include <unistd.h>

#include "pi_dma.h"
#include "pi_clk.h"

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
	uint32_t *reg;
	size_t len;

	enum dma_pacer pacer;
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

	ch->pacer = PACER_NONE;
	ch->len = DMA_CHAN_SIZE;
	ch->reg = map_peripheral(board->periph_virt_base + DMA_BASE_OFFSET + (ch->len * channel),
				  ch->len);
	if (ch->reg == MAP_FAILED) {
		goto fail;
	}

	return ch;

fail:
	dma_channel_fini(ch);
	return NULL;
}

void dma_channel_fini(struct dma_channel *ch)
{
	dma_channel_setup_pacer(ch, PACER_NONE, 0);
	ch->reg[DMA_CS] = DMA_RESET;
	usleep(10);

	if (ch->reg != MAP_FAILED) {
		munmap(ch->reg, ch->len);
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
			pcm_reg[PCM_CS_A] &= ~(1<<9);			// Disable DMA
		}
	}
	ch->pacer = pacer;

	return;
}

void dma_channel_run(struct dma_channel *ch, uint32_t cb_base_phys)
{
	// Initialise the DMA
	ch->reg[DMA_CS] = DMA_RESET;
	usleep(10);
	ch->reg[DMA_CS] = DMA_INT | DMA_END;
	ch->reg[DMA_CONBLK_AD] = cb_base_phys;
	ch->reg[DMA_DEBUG] = 7; // clear debug error flags
	ch->reg[DMA_CS] = 0x10880001;	// go, mid priority, wait for outstanding writes

	if (ch->pacer == PACER_PCM) {
		pcm_reg[PCM_CS_A] |= 1<<2;			// Enable Tx
	}
}
