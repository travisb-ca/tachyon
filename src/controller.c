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
#include "options.h"
#include "controller.h"

#define STDIN 0
#define STDOUT 1

static struct controller GCon;

/* The currently active buffer */
static int current_buf_num;
static struct buffer *current_buf;

bool run = true;

static void handle_sigwinch(siginfo_t *siginfo, int num_signals) {
	struct winsize winsize;
	int result;

	DLOG("Received SIGWINCH %d times", num_signals);

	winsize = tty_get_winsize(STDIN);
	result = buffer_set_winsize(current_buf, winsize.ws_row, winsize.ws_col);
	if (result)
		WLOG("Failed to set slave window size %d", result);
}

/*
 * Set the current buffer to the given buffer number if it exists.
 */
static void controller_set_current_buffer(int num) {
	if (GCon.buffers[num] != NULL) {
		current_buf_num = num;
		current_buf = GCon.buffers[num];
	}
}

/*
 * Create a new buffer and switch to it if successful.
 */
static void controller_new_buffer(void) {
	int buf_num;

	/* Find next available buffer number */
	if (GCon.buffers[0] != NULL) {
		buf_num = 0;
	} else {
		for (buf_num = 0; buf_num != 0; buf_num = (buf_num + 1) % CONTROLLER_MAX_BUFS) {
			if (GCon.buffers[buf_num] != NULL)
				break;
		}

		if (buf_num == 0) {
			NOTIFY("No free buffers found! Failed to create new buffer");
			return;
		}
	}

	GCon.buffers[buf_num] = buffer_init(buf_num);
	if (!GCon.buffers[buf_num]) {
		NOTIFY("Failed to create new buffer");
		return;
	}

	controller_set_current_buffer(buf_num);
}

/*
 * Change the current buffer to the next buffer, if there is one. Otherwise
 * notify the user that there is only one.
 */
static void controller_next_buffer(void) {
	int i;
	for (i = current_buf_num + 1; i != current_buf_num; i = (i + 1) % CONTROLLER_MAX_BUFS) {
		if (GCon.buffers[i] != NULL)
			break;
	}

	if (i == current_buf_num)
		NOTIFY("No other buffer!\n");
	else
		controller_set_current_buffer(i);
}

static int controller_handle_metakey(int size, char *input) {
	int bytes_eaten = 0;
	int meta_start = 0;
	int meta_end = 0;
	for (int i = 0; i < size; i++) {
		if (input[i] == CONTROL(cmd_options.keys.meta) && !(GCon.flags & CONTROLLER_IN_META)) {
			/*
			 * Received a metakey, do higher level processing and
			 * then remove the key sequence from the input
			 */
			meta_start = i;
			meta_end = i;
			bytes_eaten++;
			GCon.flags |= CONTROLLER_IN_META;
			VLOG("received meta key\n");
			continue;
		}

		if (GCon.flags & CONTROLLER_IN_META) {
			/* Just about every meta-sequence doesn't display, so
			 * eat the character. Just about every meta-sequence is
			 * only ony character and so exits meta-mode
			 * immediately, so do that by default too.
			 */
			bytes_eaten++;
			meta_end = i;
			GCon.flags &= ~CONTROLLER_IN_META;

			if (input[i] == cmd_options.keys.meta) {
				VLOG("escaping metakey\n");
				input[i - 1] = cmd_options.keys.meta;

				/* Don't consume this character, but output it */
				meta_start++;
				bytes_eaten--;
			} else if (input[i] == cmd_options.keys.buffer_create) {
				VLOG("Creating new buffer\n");
				controller_new_buffer();
			} else if (input[i] == cmd_options.keys.buffer_next) {
				VLOG("Changing to next buffer\n");
				controller_next_buffer();
			} else {
				VLOG("Ignoring unhandled meta-sequence\n");
			}

			if (meta_start < meta_end)
				memmove(input + meta_start, input + meta_end, size - meta_end);
		}
	}

	return size - bytes_eaten;
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
			result = controller_handle_metakey(result, bytes);
			if (result > 0) {
				result = buffer_output(current_buf, result, bytes);
				if (result != 0) {
					WLOG("buffer ran out of space! dropping chars");
				}
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

	current_buf_num = 0;
	current_buf = GCon.buffers[0];

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
