/*
 * Copyright (C) 2013-2014  Travis Brown (travisb@travisbrown.ca)
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
#include <sys/ioctl.h>

#include "util.h"
#include "loop.h"
#include "buffer.h"
#include "log.h"
#include "tty.h"
#include "options.h"
#include "config.h"
#include "controller.h"

#define STDIN 0
#define STDOUT 1

static struct controller GCon;

/* The currently active buffer */
static int current_buf_num;
static struct buffer *current_buf;

/* Stack which holds the stack of previous buffers the user has seen */
static int buffer_stack[CONTROLLER_MAX_BUFS - 1];

/*
 * The size of the connected terminal. Currently only used to determine the
 * size of new buffers.
 */
int terminal_rows = 80;
int terminal_cols = 24;

bool run = true;

/*
 * Manipulate the buffer stack such that the buffer we are leaving it at the
 * top of the stack and the buffer we are entering isn't on the stack at
 * all.
 */
static void bufstack_swap(int leaving, int entering) {
	int e_index = 0;

	/* Common case of the user switching back to the last used buffer */
	if (buffer_stack[0] == entering) {
		buffer_stack[0] = leaving;
		return;
	}

	/*
	 * Find the element we are going to replace, this could either be
	 * the location where entering is already in the stack, or it could
	 * be an unused entry if entering has never been entered before.
	 */
	while (e_index < ARRAY_SIZE(buffer_stack)) {
		if (buffer_stack[e_index] == entering
		    || buffer_stack[e_index] == -1)
			break;
		e_index++;
	}

	if (e_index == CONTROLLER_MAX_BUFS) {
		ELOG("failed to find empty element in buf_stack!");
		return;
	}

	memmove(&buffer_stack[1], &buffer_stack[0], e_index * sizeof(*buffer_stack));
	buffer_stack[0] = leaving;
}

/*
 * Remove the given buffer from the buffer stack. Usually because that
 * buffer has closed.
 */
static void bufstack_remove(int bufnum) {
	int i = 0;
	const int size = ARRAY_SIZE(buffer_stack);

	while (i < ARRAY_SIZE(buffer_stack) && buffer_stack[i] != bufnum)
		i++;

	if (i == CONTROLLER_MAX_BUFS) {
		ELOG("Failed to find bufnum %d to remove", bufnum);
		return;
	}

	memmove(&buffer_stack[i], &buffer_stack[i + 1], (size - i) * sizeof(*buffer_stack));
	buffer_stack[size - 1] = -1;
}

static void handle_sigwinch(siginfo_t *siginfo, int num_signals) {
	struct winsize winsize;
	int result;

	DLOG("Received SIGWINCH %d times", num_signals);

	winsize = tty_get_winsize(STDIN);

	terminal_rows = winsize.ws_row;
	terminal_cols = winsize.ws_col;

	result = buffer_set_winsize(current_buf, winsize.ws_row, winsize.ws_col);
	if (result)
		WLOG("Failed to set slave window size %d", result);
}

/*
 * Clear the window pane.
 */
static void controller_clear(int bufid) {
	const char vt100_clear_screen[] = "\033[2J";
	controller_output(bufid, sizeof(vt100_clear_screen) - 1,
			  vt100_clear_screen);
}

/*
 * Set the current buffer to the given buffer number if it exists.
 */
static void controller_set_current_buffer(unsigned int num) {
	if (GCon.buffers[num] != NULL) {
		bufstack_swap(current_buf_num, num);
		current_buf_num = num;
		current_buf = GCon.buffers[num];
	}

	controller_clear(current_buf_num);
	buffer_redraw(current_buf);
}

/*
 * Create a new buffer and switch to it if successful.
 */
static void controller_new_buffer(void) {
	unsigned int buf_num;

	/* Find next available buffer number */
	if (GCon.buffers[0] == NULL) {
		buf_num = 0;
	} else {
		for (buf_num = 1; buf_num != 0; buf_num = (buf_num + 1) % CONTROLLER_MAX_BUFS) {
			if (GCon.buffers[buf_num] == NULL)
				break;
		}

		if (buf_num == 0) {
			NOTIFY("No free buffers found! Failed to create new buffer");
			return;
		}
	}

	GCon.buffers[buf_num] = buffer_init(buf_num, terminal_rows, terminal_cols);
	if (!GCon.buffers[buf_num]) {
		NOTIFY("Failed to create new buffer");
		return;
	}

	controller_set_current_buffer(buf_num);
	handle_sigwinch(NULL, -1);
}

/*
 * Change the current buffer to the next buffer, if there is one. Otherwise
 * notify the user that there is only one.
 */
static void controller_next_buffer(void) {
	unsigned int i;
	for (i = (current_buf_num + 1) % CONTROLLER_MAX_BUFS;
	     i != current_buf_num;
	     i = (i + 1) % CONTROLLER_MAX_BUFS) {
		if (GCon.buffers[i] != NULL)
			break;
	}

	if (i == current_buf_num)
		NOTIFY("No other buffer!\n");
	else
		controller_set_current_buffer(i);
}

static int unsigned_mod_less_one(unsigned int i, unsigned int m) {
	if (i == 0)
		return m - 1;
	else
		return i - 1;
}

/*
 * Change the current buffer to the previous buffer, if there is one. Otherwise
 * notify the user that there is only one.
 */
static void controller_prev_buffer(void) {
	unsigned int i;
	for (i = unsigned_mod_less_one(current_buf_num, CONTROLLER_MAX_BUFS);
	     i != current_buf_num;
	     i = unsigned_mod_less_one(i, CONTROLLER_MAX_BUFS)) {
		if (GCon.buffers[i] != NULL)
			break;
	}

	if (i == current_buf_num)
		NOTIFY("No other buffer!\n");
	else
		controller_set_current_buffer(i);
}

/*
 * Change the current buffer to the buffer the user was previously looking
 * at. If there is no other buffer notify the user.
 */
static void controller_last_buffer(void) {
	if (buffer_stack[0] == -1)
		NOTIFY("No other buffer!\n");
	else
		controller_set_current_buffer(buffer_stack[0]);
}

/*
 * Change the current buffer to the given buffer number if it exists, otherwise just notify the user.
 */
static void controller_goto_buffer(int bufnum) {
	if (bufnum >= CONTROLLER_MAX_BUFS || !GCon.buffers[bufnum])
		NOTIFY("Buffer %d doesn't exist", bufnum);
	else
		controller_set_current_buffer(bufnum);
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
			VLOG("received meta key");
			continue;
		}

		if (GCon.flags & CONTROLLER_IN_META) {
			/* Just about every meta-sequence doesn't display, so
			 * eat the character. Just about every meta-sequence is
			 * only ony character and so exits meta-mode
			 * immediately, so do that by default too.
			 */
			bytes_eaten++;
			meta_end = i + 1;
			GCon.flags &= ~CONTROLLER_IN_META;

			if (input[i] == cmd_options.keys.meta) {
				VLOG("escaping metakey");
				input[i - 1] = cmd_options.keys.meta;

				/* Don't consume this character, but output it */
				meta_start++;
				bytes_eaten--;
			} else if (input[i] == cmd_options.keys.buffer_create) {
				VLOG("Creating new buffer");
				controller_new_buffer();
			} else if (input[i] == cmd_options.keys.buffer_next) {
				VLOG("Changing to next buffer");
				controller_next_buffer();
			} else if (input[i] == cmd_options.keys.buffer_prev) {
				VLOG("Changing to prev buffer");
				controller_prev_buffer();
			} else if (input[i] == cmd_options.keys.buffer_last) {
				VLOG("Changing to last buffer");
				controller_last_buffer();
			} else if (input[i] == cmd_options.keys.buffer_0) {
				VLOG("Changing to buffer 0");
				controller_goto_buffer(0);
			} else if (input[i] == cmd_options.keys.buffer_1) {
				VLOG("Changing to buffer 1");
				controller_goto_buffer(1);
			} else if (input[i] == cmd_options.keys.buffer_2) {
				VLOG("Changing to buffer 2");
				controller_goto_buffer(2);
			} else if (input[i] == cmd_options.keys.buffer_3) {
				VLOG("Changing to buffer 3");
				controller_goto_buffer(3);
			} else if (input[i] == cmd_options.keys.buffer_4) {
				VLOG("Changing to buffer 4");
				controller_goto_buffer(4);
			} else if (input[i] == cmd_options.keys.buffer_5) {
				VLOG("Changing to buffer 5");
				controller_goto_buffer(5);
			} else if (input[i] == cmd_options.keys.buffer_6) {
				VLOG("Changing to buffer 6");
				controller_goto_buffer(6);
			} else if (input[i] == cmd_options.keys.buffer_7) {
				VLOG("Changing to buffer 7");
				controller_goto_buffer(7);
			} else if (input[i] == cmd_options.keys.buffer_8) {
				VLOG("Changing to buffer 8");
				controller_goto_buffer(8);
			} else if (input[i] == cmd_options.keys.buffer_9) {
				VLOG("Changing to buffer 9");
				controller_goto_buffer(9);
			} else {
				VLOG("Ignoring unhandled meta-sequence");
			}

			if (meta_start < meta_end) {
				memmove(input + meta_start, input + meta_end, size - meta_end);
				VLOG("skipped meta, left with '%s'", input);
			}

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
	struct winsize winsize;
	int result;

	winsize = tty_get_winsize(STDIN);
	terminal_rows = winsize.ws_row;
	terminal_cols = winsize.ws_col;

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

	GCon.buffers[0] = buffer_init(0, terminal_rows, terminal_cols);
	if (!GCon.buffers[0]) {
		result = ENOMEM;
		goto out_deregister;
	}

	controller_set_current_buffer(0);

	for (int i = 0; i < ARRAY_SIZE(buffer_stack); i++)
		buffer_stack[i] = -1;

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
int controller_output(int bufid, int size, const char *buf) {
	if (size > sizeof(GCon.buf_out) - GCon.buf_out_used)
		return EAGAIN;

	/* If this isn't for the current buffer don't output it */
	if (bufid != current_buf_num)
		return 0;

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
	int nextbuf = -1;
	unsigned int i;

	buffer_free(GCon.buffers[bufid]);
	GCon.buffers[bufid] = NULL;

	if (buffer_stack[0] != -1) {
		/* Jump to the previous buffer in the stack */
		nextbuf = buffer_stack[0];
	} else {
		/* Empty stack, just choose the next buffer */
		for (i = (bufid + 1) % CONTROLLER_MAX_BUFS;
		     i != bufid;
		     i = (i + 1) % CONTROLLER_MAX_BUFS) {
			if (GCon.buffers[i] != NULL) {
				nextbuf = i;
				break;
			}
		}
	}

	if (nextbuf != -1) {
		controller_set_current_buffer(nextbuf);
		bufstack_remove(bufid);
	} else {
		/*
		 * We have no buffers remaining, exit.
		 */
		run = false;
	}
}
