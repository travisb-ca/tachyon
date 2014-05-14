/*
 * Copyright (C) 2013  Travis Brown (travisb@travisbrown.ca)
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
/*
 * Header for the functions related to the controlling tty which the user is
 * sitting in front of.
 */
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "loop.h"
#include "buffer.h"
#include "config.h"

struct controller {
	struct loop_fd in; /* stdin */
	struct loop_fd out; /* stdout */

	int flags;
#define CONTROLLER_IN_META (1 << 0) /* Input processing is in the middle of processing meta keys */

	int buf_out_used;
	char buf_out[CONTROLLER_BUF_SIZE];

	struct buffer *buffers[CONTROLLER_BUF_SIZE];
};

bool run;

int controller_init(void);
int controller_output(int bufid, int size, const char *buf);
void controller_buffer_exiting(int bufid);

#endif
