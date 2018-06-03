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
#ifndef __PI_BACKEND_H__
#define __PI_BACKEND_H__
#include "pi_hw/pi_util.h"

struct pi_backend;

struct pi_backend *pi_backend_create(struct board_cfg *board);
void pi_backend_destroy(struct pi_backend *be);

int pi_backend_wait_fence(struct pi_backend *be, int timeout_millis,
			  int sleep_millis);

#endif /* __PI_BACKEND_H__ */
