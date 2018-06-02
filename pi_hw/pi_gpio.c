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

#include "pi_gpio.h"

#define GPIO_BASE_OFFSET	0x00200000
#define GPIO_LEN		0x100

#define GPIO_FSEL0		(0x00/4)
#define GPIO_SET0		(0x1c/4)
#define GPIO_CLR0		(0x28/4)
#define GPIO_LEV0		(0x34/4)
#define GPIO_PULLEN		(0x94/4)
#define GPIO_PULLCLK		(0x98/4)

#define MAX_GPIO 54
struct gpio_dev {
	uint32_t *reg;
	size_t len;

	uint64_t restore_mask;
	enum gpio_mode restore[MAX_GPIO];
};

struct gpio_dev *gpio_init(struct board_cfg *board)
{
	struct gpio_dev *dev = calloc(1, sizeof(*dev));
	if (!dev) {
		return NULL;
	}

	dev->len = GPIO_LEN;
	dev->reg = map_peripheral(board->periph_virt_base + GPIO_BASE_OFFSET,
				  dev->len);
	if (dev->reg == MAP_FAILED) {
		goto fail;
	}

	return dev;

fail:
	gpio_fini(dev);
	return NULL;
}

void gpio_fini(struct gpio_dev *dev)
{
	if (dev->reg != MAP_FAILED) {
		munmap(dev->reg, dev->len);
	}
	free(dev);
}

void gpio_set_mode(struct gpio_dev *dev, int gpio, enum gpio_mode mode)
{
	uint32_t fsel = dev->reg[GPIO_FSEL0 + gpio/10];

	fsel &= ~(7 << ((gpio % 10) * 3));
	fsel |= mode << ((gpio % 10) * 3);
	dev->reg[GPIO_FSEL0 + gpio/10] = fsel;
}

enum gpio_mode gpio_get_mode(struct gpio_dev *dev, int gpio)
{
	uint32_t fsel = dev->reg[GPIO_FSEL0 + gpio/10];

	return (fsel >> ((gpio % 10) * 3)) & 7;
}

void gpio_set(struct gpio_dev *dev, uint32_t gpios)
{
	dev->reg[GPIO_SET0] = gpios;
}

void gpio_clear(struct gpio_dev *dev, uint32_t gpios)
{
	dev->reg[GPIO_CLR0] = gpios;
}
