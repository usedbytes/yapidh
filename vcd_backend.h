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
#ifndef __VCD_BACKEND_H__
#define __VCD_BACKEND_H__
#include <stdint.h>

#include "wave_gen.h"

struct vcd_backend {
	struct wave_backend base;

	int n_channels;
	int *pins;

	int time;
	uint32_t rising;
	uint32_t falling;
};

struct vcd_backend *vcd_backend_create(uint32_t pins);
void vcd_backend_fini(struct vcd_backend *be);

#endif /* __VCD_BACKEND_H__ */
