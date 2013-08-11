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
 * Functions relating to the terminal the user is sitting at.
 */

#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "util.h"
#include "loop.h"
#include "buffer.h"
#include "log.h"
#include "controller.h"

#define STDIN 0
#define STDOUT 1

struct controller global_controller;

static void controller_cb_in(struct loop_fd *fd, int revents) {
	struct controller *controller = container_of(fd, struct controller, in);
	int result;

	VLOG("controller %p in %d", controller, revents);
	if (revents & (POLLHUP | POLLERR)) {
		ELOG("controller %p has error on stdin", controller);
		controller->in.poll_flags = 0;
	}

	if (revents & (POLLIN | POLLPRI)) {
		/* read from buffer */
		char bytes[1024];
		result = read(controller->in.fd, bytes, sizeof(bytes));
		VLOG("read %d bytes from controller", result);
		if (result < 0) {
			WLOG("error reading controller %p %d %d", controller, result, errno);
		} else {
			result = buffer_output(&global_buffer, result, bytes);
			if (result != 0) {
				WLOG("buffer ran out of space! dropping chars");
			}
		}
	}
}

static void controller_cb_out(struct loop_fd *fd, int revents) {
	struct controller *controller = container_of(fd, struct controller, out);
	int result;

	VLOG("controller %p out %d", controller, revents);
	if (revents & (POLLHUP | POLLERR)) {
		ELOG("controller %p has error on stdout", controller);
		controller->out.poll_flags = 0;
	}

	if (revents & POLLOUT) {
		/* flush data to pty */
		result = write(controller->out.fd, controller->buf_out, controller->buf_out_used);
		VLOG("wrote %d bytes to controller %p", result, controller);
		if (result <= 0) {
			WLOG("error writing controller %p %d %d", controller, result, errno);
		} else {
			controller->buf_out_used -= result;
			if (controller->buf_out_used == 0)
				controller->out.poll_flags &= ~POLLOUT;
		}
	}
}

/*
 * Initialize the global controller
 *
 * Returns:
 * 0      - On success
 * ENOMEM - Failed to allocate memory to register
 */
int controller_init(void) {
	int result;

	global_controller.in.fd = STDIN;
	global_controller.in.poll_flags = POLLIN | POLLPRI;
	global_controller.in.poll_callback = controller_cb_in;

	global_controller.out.fd = STDOUT;
	global_controller.out.poll_flags = 0;
	global_controller.out.poll_callback = controller_cb_out;

	global_controller.buf_out_used = 0;

	result = loop_register((struct loop_fd *)&global_controller.in);
	if (result != 0)
		return result;

	result = loop_register((struct loop_fd *)&global_controller.out);
	if (result != 0) {
		loop_deregister((struct loop_fd *)&global_controller.in);
		return result;
	}

	return 0;
}

/*
 * Queue data to be output to stdout so the user can see it. Either all the
 * bytes or none of the bytes will be queued.
 *
 * Returns:
 * 0      - On success
 * EAGAIN - The buffer is currently full
 */
int controller_output(struct controller *controller, int size, char *buf) {
	if (size > sizeof(controller->buf_out) - controller->buf_out_used)
		return EAGAIN;

	memcpy(&controller->buf_out + controller->buf_out_used,
	       buf, size);
	controller->out.poll_flags |= POLLOUT;

	return 0;
}
