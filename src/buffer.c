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

static void buffer_cell_init(struct buffer_cell *cell)
{
	memset(cell, 0, sizeof(*cell));
}

static struct buffer_line *buffer_line_alloc(int columns)
{
	struct buffer_line *line;

	line = malloc(sizeof(*line) + columns*sizeof(*line->cells));
	if (!line)
		return NULL;

	line->next = NULL;
	line->prev = NULL;
	line->len = columns;

	for (int i = 0; i < columns; i++)
		buffer_cell_init(&line->cells[i]);

	return line;
}

static void buffer_line_free(struct buffer_line *line)
{
	free(line);
}

static void buffer_line_init(struct buffer_line *line, struct buffer_line *prev,
			     struct buffer_line *next)
{
	line->next = next;
	line->prev = prev;
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

	/* Allocate the screen/line buffer */
	buffer->vt.cols = 80;
	buffer->vt.rows = 24;
	buffer->vt.current_row = 0;
	buffer->vt.current_col = 0;

	buffer->vt.lines = malloc(buffer->vt.cols * sizeof(*buffer->vt.lines));
	if (!buffer->vt.lines)
		goto out_free_fd;

	for (int i = 0; i < buffer->vt.rows; i++) {
		buffer->vt.lines[i] = buffer_line_alloc(buffer->vt.cols);
		if (!buffer->vt.lines[i])
			goto out_free_lines;
	}
	buffer->vt.topmost = buffer->vt.lines[0];
	buffer->vt.bottommost = buffer->vt.lines[buffer->vt.rows - 1];

	buffer_line_init(buffer->vt.topmost, NULL, buffer->vt.lines[1]);
	for (int i = 1; i < buffer->vt.rows - 1; i++)
		buffer_line_init(buffer->vt.lines[i], buffer->vt.lines[i - 1], buffer->vt.lines[i + 1]);
	buffer_line_init(buffer->vt.bottommost, buffer->vt.lines[buffer->vt.cols - 2], NULL);

	result = loop_register(&buffer->fd);
	if (result != 0)
		goto out_free_lines;

	return buffer;

out_free_lines:
	if (buffer->vt.lines) {
		for (int i = 0; i < buffer->vt.cols; i++)
			free(buffer->vt.lines[i]);
	}
	free(buffer->vt.lines);
out_free_fd:
	close(buffer->fd.fd);

out_free:
	free(buffer);
	return NULL;
}

void buffer_free(struct buffer *buffer) {
	struct buffer_line *current;
	struct buffer_line *next = NULL;

	loop_deregister(&buffer->fd);
	predictor_free(&buffer->predictor);
	close(buffer->fd.fd);

	current = buffer->vt.topmost;
	if (current)
		next = current->next;
	while (current) {
		buffer_line_free(current);

		current = next;
		if (next)
			next = next->next;
	}

	free(buffer);
}

struct buffer_cell *buffer_get_cell(struct buffer *buf, unsigned int row, unsigned int col)
{
	struct buffer_line *line;

	if (row >= buf->vt.rows || col >= buf->vt.cols)
		return NULL;

	line = buf->vt.lines[row];

	if (col >= line->len)
		return NULL;

	return &line->cells[col];
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
	struct buffer_line *line;

	for (int i = 0; i < size; i++) {
		vt_interpret(buffer, buf[i]);

		if (buffer->vt.current_col == buffer->vt.cols) {
			/* End of the line, move down one */
			DLOG("End of line reached");
			buffer->vt.current_col = 0;
			buffer->vt.current_row++;

		}

		if (buffer->vt.current_row == buffer->vt.rows) {
			/* Last line in the buffer, scroll */
			DLOG("End of buffer reached");
			line = buffer_line_alloc(buffer->vt.cols);
			if (!line) {
				ELOG("Failed to allocate new line!");
				continue;
			}

			buffer_line_init(line, buffer->vt.bottommost, NULL);
			buffer->vt.bottommost->next = line;
			memmove(&buffer->vt.lines[0], &buffer->vt.lines[1],
				(buffer->vt.rows - 1) * sizeof(*buffer->vt.lines));
			buffer->vt.lines[buffer->vt.rows - 1] = line;
			buffer->vt.bottommost = line;

			buffer->vt.current_row--;
		}
	}

	return controller_output(buffer->bufid, size, buf);
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
	struct buffer_cell *cell;
	const char *vt100_goto_origin = "\033[f";
	const char *space = " ";

	controller_output(buffer->bufid, sizeof(vt100_goto_origin) - 1,
			  vt100_goto_origin);

	for (int row = 0; row < buffer->vt.rows; row++) {
		for (int col = 0; col < buffer->vt.cols; col++) {
			cell = buffer_get_cell(buffer, row, col);
			if (cell->flags & BUF_CELL_SET)
				controller_output(buffer->bufid, 1, &cell->c);
			else
				controller_output(buffer->bufid, 1, space);
		}
		if (row < buffer->vt.rows - 1)
			controller_output(buffer->bufid, 2, "\r\n");
	}
}
