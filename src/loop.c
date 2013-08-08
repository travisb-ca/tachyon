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
 * Loop which poll()s all the various fds and calls the appropriate
 * callbacks. Also handles the waiting for signals.
 */

#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "pal.h"

#include "loop.h"

#define MIN_ITEMS_SIZE 16

struct loop_item {
	struct loop_fd *fd;
};

static struct loop_item *loop_items;
static struct pollfd *fds;

/* The number of loop items actually used */
static int num_loop_items;

/* The number of item allocated, which may be less than num_loop_items */
static int max_loop_items;

static struct signal_fd {
	struct loop_fd fd;

	int write_fd;
} signal_fd;

/*
 * Register a loop_fd to be polled for.
 *
 * Returns:
 * 0      - On success
 * ENOMEM - Unable to allocate memory to register
 */
int loop_register(struct loop_fd *fd) {
	int result;
	int i;

	if (num_loop_items >= max_loop_items) {
		/* Need to allocate more memory */
		struct loop_item *new_items = NULL;
		struct pollfd *new_fds = NULL;
		int new_size;

		if (max_loop_items == 0)
			new_size = MIN_ITEMS_SIZE;
		else
			new_size = 2 * max_loop_items;

		new_items = calloc(new_size, sizeof(*loop_items));
		new_fds = calloc(new_size, sizeof(*loop_items));

		if (!new_items || !new_fds) {
			if (new_items)
				free(new_items);
			if (new_fds)
				free(new_fds);

			return ENOMEM;
		}

		memmove(new_items, loop_items, max_loop_items * sizeof(*loop_items));
		memmove(new_fds, fds, max_loop_items * sizeof(*fds));

		free(loop_items);
		loop_items = new_items;
		free(fds);
		fds = new_fds;
	}

	i = num_loop_items++;
	loop_items[i].fd = fd;
	fds[i].fd = fd->fd;
	fds[i].events = fd->poll_flags;

	return 0;
}

/*
 * Unregister a loop_fd from being processed during a poll cycle.
 *
 * Returns:
 * 0 - On success
 */
int loop_deregister(struct loop_fd *fd) {
	int i;

	for (i = 0; i < num_loop_items; i++)
		if (loop_items[i].fd == fd)
			break;

	memmove(&loop_items[i], &loop_items[num_loop_items - 1], sizeof(*loop_items));
	memmove(&fds[i], &fds[num_loop_items - 1], sizeof(*fds));

	num_loop_items--;

	return 0;
}

/*
 * This function is called from the main poll loop to handle out of band
 * signals. This is not only asynchronous unix signals, but also other out
 * of band actions requested by other parts of the program.
 */
static void process_signals(struct loop_fd *fd, int revents) {
	struct signal_fd *signal = (struct signal_fd *)fd;
	int result;
	char buf[4];

	DLOG("Received signal");

	result = read(signal->fd.fd, buf, sizeof(buf));
}

/*
 * Initialize the loop.
 *
 * Returns:
 * 0      - On success
 * EPIPE  - Unable to create signal pipe
 * ENOMEM - Unable to request memory to register signal pipe
 */
int loop_init(void) {
	int result;

	{
		int pipes[2];

		result = pipe(pipes);
		if (result == -1) {
			ELOG("Unable to create signal pipe %d", errno);
			return EPIPE;
		}

		signal_fd.fd.fd = pipes[0];
		signal_fd.write_fd = pipes[1];
		signal_fd.fd.poll_flags = POLLIN;
		signal_fd.fd.poll_callback = process_signals;

		result = loop_register((struct loop_fd *)&signal_fd);
		if (result)
			return ENOMEM;
	}

	return 0;
}

/*
 * Run the poll loop once, calling all the callbacks as necessary.
 *
 * Returns:
 * true  - Successfully ran the loop once
 * false - An error occurred
 */
bool loop_run(void) {
	int result;
	int i;

	/* Make sure the events to listen to are up to date */
	for (i = 0; i < num_loop_items; i++)
		fds[i].events = loop_items[i].fd->poll_flags;

	result = pal_poll(fds, num_loop_items, -1);
	if (result <= 0) {
		WLOG("poll failed %d %d\n", result, errno);
		return false;
	}

	for (i = 0; i < num_loop_items; i++) {
		if (fds[i].revents && loop_items[i].fd->poll_callback) {
			loop_items[i].fd->poll_callback(loop_items[i].fd, fds[i].revents);
		}
	}
}
