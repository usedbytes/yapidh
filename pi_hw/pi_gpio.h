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
#ifndef __PI_GPIO_H__
#define __PI_GPIO_H__
#include <stdint.h>

#include "pi_util.h"

struct gpio_dev;

enum gpio_mode {
	GPIO_MODE_IN = 0,
	GPIO_MODE_OUT = 1,
};

struct gpio_dev *gpio_init(struct board_cfg *board);
void gpio_fini(struct gpio_dev *dev);

void gpio_set_mode(struct gpio_dev *dev, int gpio, enum gpio_mode mode);
enum gpio_mode gpio_get_mode(struct gpio_dev *dev, int gpio);
void gpio_set(struct gpio_dev *dev, uint32_t gpios);
void gpio_clear(struct gpio_dev *dev, uint32_t gpios);

#define DBG_CHUNK_PIN   17
#define DBG_CPUTIME_PIN 18
#define DBG_FENCE_PIN   22

#ifdef DEBUG
static inline void gpio_debug_set(struct gpio_dev *dev, uint32_t pins)
{
	gpio_set(dev, pins);
}

static inline void gpio_debug_clear(struct gpio_dev *dev, uint32_t pins)
{
	gpio_clear(dev, pins);
}
#else
static inline void gpio_debug_set(struct gpio_dev *dev, uint32_t pins) { }
static inline void gpio_debug_clear(struct gpio_dev *dev, uint32_t pin) { }
#endif

#endif /* __PI_GPIO_H__ */
