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
#ifndef __GNUPLOT_BACKEND_H__
#define __GNUPLOT_BACKEND_H__

#include "wave_gen.h"

struct event {
	int val;
};

struct gnuplot_backend {
	struct wave_backend base;

	int time;
	struct event ev;
};

struct gnuplot_backend *gnuplot_backend_create();

#endif /* __GNUPLOT_BACKEND_H__ */
