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
#ifndef __PI_CLK_H__
#define __PI_CLK_H__
#include <stdint.h>

#include "pi_util.h"

struct clock_dev;

enum clock_consumer {
	CLOCK_CONSUMER_PWM,
	CLOCK_CONSUMER_PCM,
};

struct clock_dev *clock_init(struct board_cfg *board);
void clock_fini(struct clock_dev *dev);
int clock_set_rate(struct clock_dev *dev, enum clock_consumer c, uint64_t rate);

#endif /* __PI_CLK_H__ */
