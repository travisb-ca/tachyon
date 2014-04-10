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
 * Buffers represent the slave psuedo-ttys. If they are (partially) visible
 * they will be visible via windows.
 */

#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "loop.h"
#include "controller.h"
#include "util.h"
#include "log.h"
#include "tty.h"
#include "predictor.h"
#include "options.h"
#include "vt.h"
#include "buffer.h"

static void buffer_cb(struct loop_fd *fd, int revents) {
	struct buffer *buf = container_of(fd, struct buffer, fd);
	int result;

	VLOG("buffer %p %d", buf, revents);
	if (revents & (POLLHUP | POLLERR)) {
		buf->fd.poll_flags = 0;
		controller_buffer_exiting(buf->bufid);
		return;
	}

	if (revents & (POLLIN | POLLPRI)) {
		/* read from buffer */
		char bytes[1024];
		result = read(buf->fd.fd, bytes, sizeof(bytes));
		VLOG("read %d bytes from buffer", result);
		if (result < 0) {
			WLOG("error reading buffer %p %d %d", buf, result, errno);
		} else {
			result = predictor_learn(&buf->predictor, buf, result, bytes);
			if (result != 0) {
				WLOG("controller ran out of space! dropping chars");
			}
		}
	}

	if (revents & POLLOUT) {
		/* flush data to pty */
		result = write(buf->fd.fd, buf->buf_out, buf->buf_out_used);
		VLOG("wrote %d bytes to buffer %p", result, buf);
		if (result <= 0) {
			WLOG("error writing buffer %p %d %d", buf, result, errno);
		} else {
			buf->buf_out_used -= result;
			if (buf->buf_out_used == 0)
				buf->fd.poll_flags &= ~POLLOUT;
		}
	}
}

/*
 * Initialize a buffer.
 *
 * Returns:
 * A struct buffer * on success
 * NULL on failure
 */
struct buffer *buffer_init(int bufid) {
	struct buffer *buffer;
	int result;

	buffer = malloc(sizeof(*buffer));
	if (!buffer)
		return NULL;

	memset(buffer, 0, sizeof(*buffer));

	result = predictor_init(&buffer->predictor);
	if (result)
		goto out_free;

	buffer->bufid = bufid;
	buffer->fd.poll_flags = POLLIN | POLLPRI;
	buffer->fd.poll_callback = buffer_cb;
	buffer->fd.fd = tty_new(cmd_options.new_buf_command, bufid);

	if (buffer->fd.fd < 0)
		goto out_free;

	if (vt_init(&buffer->vt))
		goto out_free_fd;

	result = loop_register(&buffer->fd);
	if (result != 0)
		goto out_free_fd;

	return buffer;

out_free_fd:
	close(buffer->fd.fd);

out_free:
	free(buffer);
	return NULL;
}

void buffer_free(struct buffer *buffer) {
	loop_deregister(&buffer->fd);
	predictor_free(&buffer->predictor);
	close(buffer->fd.fd);

	vt_free(&buffer->vt);

	free(buffer);
}

int buffer_set_winsize(struct buffer *buf, int rows, int cols) {
	tty_set_winsize(buf->fd.fd, rows, cols);
	return 1;
#if 0
	if (rows != buf->rows || cols != buf->cols) {
		struct buffer cells;
		struct buffer_cell *oldcell;
		struct buffer_cell *newcell;

		cells.rows = rows;
		cells.cols = cols;

		cells.cells = calloc(rows * cols, sizeof(*cells.cells));
		if (!cells.cells) {
			ELOG("unable to allocate memory to change buffer size");
			goto out;
		}

		for (unsigned int row = 0; row < rows && row < buf->rows; row++) {
			for (unsigned int col = 0; col < cols && col < buf->cols; col++) {
				oldcell = get_cell(buf, row, col);
				newcell = get_cell(&cells, row, col);

				*newcell = *oldcell;
			}
		}

		free(buf->cells);
		buf->cells = cells.cells;

		buf->rows = rows;
		buf->cols = cols;
	}

	/*
	 * Always set the size of the underlying tty, even if we didn't
	 * change our size, to ensure that the signal is passed through.
	 */
out:
#endif
}

int buffer_input(struct buffer *buffer, int size, char *buf) {
	int result = controller_output(buffer->bufid, size, buf);

	for (int i = 0; i < size; i++)
		vt_interpret(buffer, buf[i]);

	return result;
}

static int _buffer_output(struct buffer *buffer, int size, char *buf) {
	if (size > sizeof(buffer->buf_out) - buffer->buf_out_used)
		return EAGAIN;

	memcpy(&buffer->buf_out + buffer->buf_out_used,
	       buf, size);
	buffer->buf_out_used += size;
	buffer->fd.poll_flags |= POLLOUT;

	return 0;
}

/*
 * Queue data to be output to the slave so the pty process can see it.
 * Either all the bytes or none of the bytes will be queued.
 *
 * Returns:
 * 0      - On success
 * EAGAIN - The buffer is currently full
 */
int buffer_output(struct buffer *buffer, int size, char *buf) {
	return predictor_output(&buffer->predictor, buffer, size, buf,
				_buffer_output);
}

/*
 * Redraw all the buffer contents to its controller
 */
void buffer_redraw(struct buffer *buffer) {
	struct vt_cell *cell;
	const char vt100_goto_origin[] = "\033[f";
	const char space[] = " ";
	char buf[16];
	int len;

	controller_output(buffer->bufid, sizeof(vt100_goto_origin) - 1,
			  vt100_goto_origin);

	for (int row = 0; row < buffer->vt.rows; row++) {
		for (int col = 0; col < buffer->vt.cols; col++) {
			cell = vt_get_cell(buffer, row, col);
			if (cell->flags & BUF_CELL_SET) {
				if (cell->style & VT_STYLE_BOLD)
					controller_output(buffer->bufid, 4, "\033[1m");
				if (cell->style & VT_STYLE_UNDERSCORE)
					controller_output(buffer->bufid, 4, "\033[4m");
				if (cell->style & VT_STYLE_BLINK)
					controller_output(buffer->bufid, 4, "\033[5m");
				if (cell->style & VT_STYLE_REVERSE)
					controller_output(buffer->bufid, 4, "\033[7m");

				controller_output(buffer->bufid, 1, &cell->c);

				if (cell->style != 0)
					controller_output(buffer->bufid, 4, "\033[0m");
			} else {
				controller_output(buffer->bufid, 1, space);
			}
		}
		if (row < buffer->vt.rows - 1)
			controller_output(buffer->bufid, 2, "\r\n");
	}

	len = snprintf(buf, sizeof(buf), "\033[%d;%df", buffer->vt.current.row + 1,
		       buffer->vt.current.col + 1);
	controller_output(buffer->bufid, len, buf);

}
