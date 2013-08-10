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
#ifndef LOOP_H
#define LOOP_H

#include <stdbool.h>

struct loop_fd {
	int fd;

	/* Flags to pass to poll for this fd */
	int poll_flags;

	/* Will be called when the socket is ready. revents is as returned by poll */
	void (*poll_callback)(struct loop_fd *fd, int revents);
};

typedef void (*loop_signal_callback)(siginfo_t *siginfo, int num_signals);

bool loop_run(void);
int loop_init(void);
int loop_register(struct loop_fd *fd);
int loop_deregister(struct loop_fd *fd);
void loop_register_signal(int signal, loop_signal_callback callback);

#endif
