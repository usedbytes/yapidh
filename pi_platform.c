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
#include <unistd.h>

#include "pi_backend.h"
#include "pi_hw/pi_gpio.h"
#include "pi_hw/pi_util.h"
#include "platform.h"

struct platform {
	struct board_cfg board;
	struct gpio_dev *gpio;
	struct pi_backend *be;
};

void platform_fini(struct platform *p)
{
	if (p->be) {
		pi_backend_destroy(p->be);
	}
	if (p->gpio) {
		gpio_fini(p->gpio);
	}
	free(p);
}

struct platform *platform_init(uint32_t pins)
{
	int ret, i;
	struct platform *p = calloc(1, sizeof(*p));
	if (!p) {
		return NULL;
	}

	ret = get_model_and_revision(&p->board);
	if (ret < 0) {
		goto fail;
	}

	printf("Periph phys: %08x\n", p->board.periph_phys_base);
	printf("Periph virt: %08x\n", p->board.periph_virt_base);
	printf("Dram phys: %08x\n", p->board.dram_phys_base);
	printf("mem_flag: %08x\n", p->board.mem_flag);

	p->gpio = gpio_init(&p->board);
	if (!p->gpio) {
		fprintf(stderr, "Couldn't get GPIO\n");
		goto fail;
	}

	for (i = 0; i < 32; i++) {
		if (!(pins & (1 << i))) {
			continue;
		}
		gpio_set_mode(p->gpio, i, GPIO_MODE_OUT);
		gpio_clear(p->gpio, (1 << i));
	}

#ifdef DEBUG
	gpio_set_mode(p->gpio, DBG_CHUNK_PIN, GPIO_MODE_OUT);
	gpio_clear(p->gpio, (1 << DBG_CHUNK_PIN));

	gpio_set_mode(p->gpio, DBG_CPUTIME_PIN, GPIO_MODE_OUT);
	gpio_clear(p->gpio, (1 << DBG_CPUTIME_PIN));

	gpio_set_mode(p->gpio, DBG_FENCE_PIN, GPIO_MODE_OUT);
	gpio_clear(p->gpio, (1 << DBG_FENCE_PIN));

	usleep(100);
#endif

	p->be = pi_backend_create(&p->board, p->gpio);
	if (!p->be) {
		fprintf(stderr, "Couldn't get backend\n");
		goto fail;
	}

	return p;

fail:
	platform_fini(p);
	return NULL;
}

struct wave_backend *platform_get_backend(struct platform *p)
{
	return (struct wave_backend *)p->be;
}

int platform_sync(struct platform *p, int timeout_millis) {
	int ret;
	gpio_debug_set(p->gpio, 1 << DBG_FENCE_PIN);
	ret = pi_backend_wait_fence(p->be, timeout_millis, 4);
	gpio_debug_clear(p->gpio, 1 << DBG_FENCE_PIN);
	return ret;
}

void platform_dump(struct platform *p) {
	pi_backend_dump(p->be);
}
