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
#ifndef __PI_UTIL_H__
#define __PI_UTIL_H__
#include <stdlib.h>
#include <stdint.h>

#define BUS_TO_PHYS(x) ((x)&~0xC0000000)

struct board_cfg {
	uint32_t periph_phys_base;
	uint32_t periph_virt_base;
	uint32_t dram_phys_base;
	uint32_t mem_flag;
};

int get_model_and_revision(struct board_cfg *board);

uint32_t *map_peripheral(uint32_t base, size_t len);

struct phys {
	int handle;		/* From mbox_open() */
	uint32_t size;		/* Required size */
	unsigned mem_ref;	/* From mem_alloc() */
	unsigned bus_addr;	/* From mem_lock() */
	unsigned phys_addr;	/* From mem_lock() */
	uint8_t *virt_addr;	/* From mapmem() */
};

struct phys *phys_alloc(struct board_cfg *board, size_t len);
void phys_free(struct phys *p);

uint32_t phys_virt_to_phys(struct phys *phys, void *virt);
uint32_t phys_virt_to_bus(struct phys *phys, void *virt);

#endif /* __PI_UTIL_H__ */
