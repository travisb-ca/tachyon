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
 * Functions relating to the terminal the user is sitting at. This also
 * includes multiplexing.
 */

#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/signal.h>
#include <termios.h>

#include "util.h"
#include "loop.h"
#include "buffer.h"
#include "log.h"
#include "tty.h"
#include "controller.h"

#define STDIN 0
#define STDOUT 1

static struct controller GCon;

bool run = true;

static void handle_sigwinch(siginfo_t *siginfo, int num_signals) {
	struct winsize winsize;
	int result;

	DLOG("Received SIGWINCH %d times", num_signals);

	winsize = tty_get_winsize(0);
	result = buffer_set_winsize(GCon.buffers[0], winsize.ws_row, winsize.ws_col);
	if (result)
		WLOG("Failed to set slave window size %d", result);
}

static void controller_cb_in(struct loop_fd *fd, int revents) {
	struct controller *controller = container_of(fd, struct controller, in);
	int result;

	VLOG("controller %p in %d", controller, revents);
	if (revents & (POLLHUP | POLLERR)) {
		ELOG("controller %p has error on stdin", controller);
		controller->in.poll_flags = 0;
		exit(0);
	}

	if (revents & (POLLIN | POLLPRI)) {
		/* read from buffer */
		char bytes[1024];
		result = read(controller->in.fd, bytes, sizeof(bytes));
		VLOG("read %d bytes from controller", result);
		if (result < 0) {
			WLOG("error reading controller %p %d %d", controller, result, errno);
		} else {
			result = buffer_output(GCon.buffers[0], result, bytes);
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
		exit(0);
	}

	if (revents & POLLOUT) {
		/* flush data to pty */
		result = write(controller->out.fd, controller->buf_out, controller->buf_out_used);
		VLOG("wrote %d bytes to controller %p", result, controller);
		if (result < 0) {
			WLOG("error writing controller %p %d %d", controller, result, errno);
		} else if (result == 0) {
			/* The out fd closed */
			exit(0);
		} else {
			controller->buf_out_used -= result;
			if (controller->buf_out_used == 0)
				controller->out.poll_flags &= ~POLLOUT;
		}
	}
}

/*
 * Initialize the global controller, this includes the first buffer.
 *
 * Returns:
 * 0      - On success
 * ENOMEM - Failed to allocate memory to register
 */
int controller_init(void) {
	int result;

	GCon.in.fd = STDIN;
	GCon.in.poll_flags = POLLIN | POLLPRI;
	GCon.in.poll_callback = controller_cb_in;

	GCon.out.fd = STDOUT;
	GCon.out.poll_flags = 0;
	GCon.out.poll_callback = controller_cb_out;

	GCon.buf_out_used = 0;

	result = loop_register((struct loop_fd *)&GCon.in);
	if (result != 0)
		return result;

	result = loop_register((struct loop_fd *)&GCon.out);
	if (result != 0)
		goto out_deregister;

	GCon.buffers[0] = buffer_init(0);
	if (!GCon.buffers[0]) {
		result = ENOMEM;
		goto out_deregister;
	}

	loop_register_signal(SIGWINCH, handle_sigwinch);

	/* Force the window size of the slave */
	handle_sigwinch(NULL, -1);

	return 0;

out_deregister:
	loop_deregister((struct loop_fd *)&GCon.in);
	return result;
}

/*
 * Queue data to be output to stdout so the user can see it. Either all the
 * bytes or none of the bytes will be queued.
 *
 * Returns:
 * 0      - On success
 * EAGAIN - The buffer is currently full
 */
int controller_output(int bufid, int size, char *buf) {
	if (size > sizeof(GCon.buf_out) - GCon.buf_out_used)
		return EAGAIN;

	memcpy(GCon.buf_out + GCon.buf_out_used,
	       buf, size);
	GCon.buf_out_used += size;
	GCon.out.poll_flags |= POLLOUT;

	return 0;
}

/*
 * Tell the controller that the given buffer is exiting, usually because the underlying shell has terminated.
 *
 * After this function is called the buffer is no longer valid.
 */
void controller_buffer_exiting(int bufid) {
	run = false;
}
