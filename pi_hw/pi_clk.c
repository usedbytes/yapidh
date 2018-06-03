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
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "pi_clk.h"

#define CLK_BASE_OFFSET	        0x00101000
#define CLK_LEN			0xA8

struct clock_dev {
	uint32_t *reg;
	size_t len;
};

struct clock_dev *clock_init(struct board_cfg *board)
{
	struct clock_dev *dev = calloc(1, sizeof(*dev));
	if (!dev) {
		return NULL;
	}

	dev->len = CLK_LEN;
	dev->reg = map_peripheral(board->periph_virt_base + CLK_BASE_OFFSET,
				  dev->len);
	if (dev->reg == MAP_FAILED) {
		goto fail;
	}

	return dev;

fail:
	clock_fini(dev);
	return NULL;

}

void clock_fini(struct clock_dev *dev)
{
	if (dev->reg != MAP_FAILED) {
		munmap(dev->reg, dev->len);
	}
	free(dev);
}

int clock_set_rate(struct clock_dev *dev, enum clock_consumer c, uint64_t rate)
{
	uint32_t base;
	double divisor;
	uint32_t integer;
	uint32_t frac;

	switch (c) {
	case CLOCK_CONSUMER_PWM:
		base = 40;
		break;
	case CLOCK_CONSUMER_PCM:
		base = 38;
		break;
	}

	divisor = 500000000.0 / rate;
	if (divisor >= 8192) {
		return -1;
	}

	integer = (uint32_t)divisor;
	frac = (uint32_t)divisor * (1.0 / (1 << 13));

	dev->reg[base] = 0x5A000006; // Source=PLLD (500MHz)
	usleep(100);
	dev->reg[base + 1] = 0x5A000000 | (integer << 12) | frac;
	usleep(100);
	dev->reg[base] = 0x5A000016; // Source=PLLD and enable
	usleep(100);

	return 0;
}
