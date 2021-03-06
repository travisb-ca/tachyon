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
 * This file contains all the interpretations of terminal characters.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "log.h"
#include "options.h"
#include "buffer.h"
#include "util.h"
#include "vt.h"

#define DEFAULT(func) \
	default: (func)(buffer, cell, c); break

#define HANDLE(ch, func) \
	case (ch): (func)(buffer, cell, c); break

enum {
	MODE_NORMAL,
	MODE_ESCAPE,
	MODE_CSI,
	MODE_OSC,
	MODE_NUM
} vt_mode;

typedef void (*mode_fn)(struct buffer *buffer, struct vt_cell *cell, char c);

struct terminal_def {
	int max_mode;
	mode_fn modes[];
};

static void vt_line_init(struct vt_line *line, struct vt_line *prev,
			     struct vt_line *next) {
	line->next = next;
	line->prev = prev;
}

static void vt_line_free(struct vt_line *line) {
	free(line);
}

static void vt_cell_init(struct vt_cell *cell) {
	memset(cell, 0, sizeof(*cell));
}

static struct vt_line *vt_line_alloc(int columns) {
	struct vt_line *line;

	line = malloc(sizeof(*line) + columns*sizeof(*line->cells));
	if (!line)
		return NULL;

	line->next = NULL;
	line->prev = NULL;
	line->len = columns;

	for (int i = 0; i < columns; i++)
		vt_cell_init(&line->cells[i]);

	return line;
}

/*
 * Initial state of the program modifiable state
 */
void static vt_reset_state(struct vt *vt) {
	vt->current.row = 0;
	vt->current.col = 0;
	vt->current.flags = 0;
	vt->saved = vt->current;

	vt->flags = VT_FL_AUTOSCROLL;
	vt->vt_mode = MODE_NORMAL;

	memset(&vt->window_title, 0, sizeof(vt->window_title));
	memset(&vt->icon_name, 0, sizeof(vt->icon_name));

	vt->params.len = 0;

	for (int i = 0; i < MAX_COLUMNS; i += 8)
		BITMAP_SETBIT(&vt->current.tabstops, i, 1);
}

/*
 * Initialize the virtual terminal to a default state.
 *
 * Returns:
 * 0      - Success
 * ENOMEM - Failed to allocate necessary memory
 */
int vt_init(struct vt *vt, int rows, int cols) {
	vt->rows = rows;
	vt->cols = cols;
	vt_reset_state(vt);

	vt->lines = malloc(vt->cols * sizeof(*vt->lines));
	if (!vt->lines)
		goto err;

	for (int i = 0; i < vt->rows; i++) {
		vt->lines[i] = vt_line_alloc(vt->cols);
		if (!vt->lines[i])
			goto err_free_lines;
	}
	vt->topmost = vt->lines[0];
	vt->bottommost = vt->lines[vt->rows - 1];

	vt_line_init(vt->topmost, NULL, vt->lines[1]);
	for (int i = 1; i < vt->rows - 1; i++)
		vt_line_init(vt->lines[i], vt->lines[i - 1], vt->lines[i + 1]);
	vt_line_init(vt->bottommost, vt->lines[vt->cols - 2], NULL);

	return 0;

err_free_lines:
	if (vt->lines) {
		for (int i = 0; i < vt->cols; i++)
			free(vt->lines[i]);
	}
	free(vt->lines);

err:
	return ENOMEM;
}

void vt_free(struct vt *vt) {
	struct vt_line *current;
	struct vt_line *next = NULL;

	current = vt->topmost;
	if (current)
		next = current->next;
	while (current) {
		vt_line_free(current);

		current = next;
		if (next)
			next = next->next;
	}
}

struct vt_cell *vt_get_cell(struct buffer *buf, unsigned int row, unsigned int col) {
	struct vt_line *line;

	if (row >= buf->vt.rows || col >= buf->vt.cols)
		return NULL;

	line = buf->vt.lines[row];

	if (col >= line->len)
		return NULL;

	return &line->cells[col];
}

static void vt_scroll_up(struct buffer *buffer) {
	struct vt *vt = &buffer->vt;
	struct vt_line *line;
	bool need_redraw = true;

	line = vt->lines[vt->rows - 1]->next;
	if (!line) {
		/* No lines below in the scrollback, create a new one */
		line = vt_line_alloc(vt->cols);
		if (!line) {
			ELOG("Failed to allocate new line!");
			return;
		}

		vt_line_init(line, vt->bottommost, NULL);
		vt->bottommost->next = line;

		need_redraw = false;
	}

	memmove(&vt->lines[0], &vt->lines[1],
		(vt->rows - 1) * sizeof(*vt->lines));
	vt->lines[vt->rows - 1] = line;
	vt->bottommost = line;

	/* Ensure that the newly visible line is displayed to the user */
	if (need_redraw)
		buffer_redraw(buffer);
}

static void vt_scroll_down(struct buffer *buffer) {
	struct vt *vt = &buffer->vt;
	struct vt_line *line;
	bool need_redraw = true;

	line = vt->lines[0]->prev;
	if (!line) {
		/* At the top of the scroll back, create a new line and insert it */
		line = vt_line_alloc(vt->cols);
		if (!line) {
			ELOG("Failed to allocate new line!");
			return;
		}

		vt_line_init(line, NULL, vt->lines[0]);
		vt->topmost->prev = line;

		need_redraw = false;
	}

	memmove(&vt->lines[1], &vt->lines[0],
		(vt->rows - 1) * sizeof(*vt->lines));
	vt->lines[0] = line;
	vt->topmost = line;

	/* Ensure that the newly visible line is displayed to the user */
	if (need_redraw)
		buffer_redraw(buffer);
}

static void ignore(struct buffer *buffer, struct vt_cell *cell, char c) {}

static void normal_chars(struct buffer *buffer, struct vt_cell *cell, char c) {
	struct vt *vt = &buffer->vt;

	cell->c = c;
	cell->flags = vt->current.flags | VT_FLAG_CELL_SET;
	vt->current.col++;
}

static void normal_backspace(struct buffer *buffer, struct vt_cell *cell, char c) {
	if (buffer->vt.current.col > 0)
		buffer->vt.current.col--;
}

static void normal_tab(struct buffer *buffer, struct vt_cell *cell, char c) {
	/*
	 * If the tabstop would move beyond the edge of the screen overwrite
	 * the last character and don't move.
	 */
	struct vt *vt = &buffer->vt;
	int tabstop;
	int i;

	tabstop = vt->cols - 1;
	for (i = vt->current.col + 1; i < vt->cols - 1; i++) {
		if(BITMAP_GETBIT(&vt->current.tabstops, i)) {
			tabstop = i;
			break;
		}
	}

	for (i = vt->current.col; i < tabstop; i++) {
		cell = vt_get_cell(buffer, vt->current.row, i);
		if (cell)
			cell->c = ' ';
	}

	buffer->vt.current.col = tabstop;
}

static void normal_newline(struct buffer *buffer, struct vt_cell *cell, char c) {
	buffer->vt.current.row++;
}

static void normal_linefeed(struct buffer *buffer, struct vt_cell *cell, char c) {
	buffer->vt.current.col = 0;
}

static void normal_escape(struct buffer *buffer, struct vt_cell *cell, char c) {
	buffer->vt.vt_mode = MODE_ESCAPE;
}

static void normal_mode(struct buffer *buffer, struct vt_cell *cell, char c) {
	switch (c) {
		DEFAULT(normal_chars);
		HANDLE(0x00, ignore);
		HANDLE(0x01, ignore);
		HANDLE(0x02, ignore);
		HANDLE(0x03, ignore);
		HANDLE(0x04, ignore);
		HANDLE(0x05, ignore);
		HANDLE(0x06, ignore);
		HANDLE('\a', ignore);
		HANDLE('\b', normal_backspace);
		HANDLE('\t', normal_tab);
		HANDLE('\n', normal_newline);
		HANDLE(0x0b, ignore);
		HANDLE(0x0c, ignore);
		HANDLE('\r', normal_linefeed);
		HANDLE(0x0e, ignore);
		HANDLE(0x0f, ignore);
		HANDLE(0x10, ignore);
		HANDLE(0x11, ignore);
		HANDLE(0x12, ignore);
		HANDLE(0x13, ignore);
		HANDLE(0x14, ignore);
		HANDLE(0x15, ignore);
		HANDLE(0x16, ignore);
		HANDLE(0x17, ignore);
		HANDLE(0x18, ignore);
		HANDLE(0x19, ignore);
		HANDLE(0x1a, ignore);
		HANDLE(0x1b, normal_escape);
		HANDLE(0x1c, ignore);
		HANDLE(0x1d, ignore);
		HANDLE(0x1e, ignore);
		HANDLE(0x1f, ignore);
		HANDLE(0x7f, ignore);
	}
}

static void escape_exit(struct buffer *buffer, struct vt_cell *cell, char c) {
	buffer->vt.vt_mode = MODE_NORMAL;
}

static void escape_save_cursor(struct buffer *buffer, struct vt_cell *cell, char c) {
	buffer->vt.saved = buffer->vt.current;
	buffer->vt.vt_mode = MODE_NORMAL;
}

static void escape_restore_cursor(struct buffer *buffer, struct vt_cell *cell, char c) {
	buffer->vt.current = buffer->vt.saved;
	buffer->vt.vt_mode = MODE_NORMAL;
}

static void escape_cursor_down(struct buffer *buffer, struct vt_cell *cell, char c) {
	struct vt *vt = &buffer->vt;

	vt->current.row++;
	vt->vt_mode = MODE_NORMAL;
}

static void escape_next_line(struct buffer *buffer, struct vt_cell *cell, char c) {
	struct vt *vt = &buffer->vt;

	vt->current.row++;
	vt->current.col = 0;
	vt->vt_mode = MODE_NORMAL;
}

static void escape_tabstop_set(struct buffer *buffer, struct vt_cell *cell, char c) {
	struct vt *vt = &buffer->vt;

	BITMAP_SETBIT(&vt->current.tabstops, vt->current.col, 1);

	vt->vt_mode = MODE_NORMAL;
}

static void escape_cursor_up(struct buffer *buffer, struct vt_cell *cell, char c) {
	struct vt *vt = &buffer->vt;

	if (vt->current.row == 0)
		vt_scroll_down(buffer);
	else
		vt->current.row--;

	vt->vt_mode = MODE_NORMAL;
}

static void clear_vt_params(struct vt *vt) {
	vt->params.len = 0;
	memset(vt->params.chars, 0, sizeof(vt->params.chars));
}

static void escape_csi(struct buffer *buffer, struct vt_cell *cell, char c) {
	buffer->vt.vt_mode = MODE_CSI;
	clear_vt_params(&buffer->vt);
}

static void escape_osc(struct buffer *buffer, struct vt_cell *cell, char c) {
	buffer->vt.vt_mode = MODE_OSC;
	clear_vt_params(&buffer->vt);
}

static void escape_reset_to_initial(struct buffer *buffer, struct vt_cell *cell, char c) {
	struct vt *vt = &buffer->vt;
	vt_reset_state(vt);

	vt->vt_mode = MODE_NORMAL;
}

static void escape_mode(struct buffer *buffer, struct vt_cell *cell, char c) {
	switch (c) {
		DEFAULT(escape_exit);
		HANDLE('7', escape_save_cursor);
		HANDLE('8', escape_restore_cursor);
		HANDLE('D', escape_cursor_down);
		HANDLE('E', escape_next_line);
		HANDLE('H', escape_tabstop_set);
		HANDLE('M', escape_cursor_up);
		HANDLE('[', escape_csi);
		HANDLE(']', escape_osc);
		HANDLE('c', escape_reset_to_initial);
	}
}

static void collect_params(struct buffer *buffer, struct vt_cell *cell, char c) {
	struct vt *vt = &buffer->vt;

	if (vt->params.len < sizeof(vt->params.chars) - 1)
		vt->params.chars[vt->params.len++] = c;
}

static void csi_clear_screen(struct buffer *buffer, struct vt_cell *cell, char c) {
	struct vt *vt = &buffer->vt;

	if (vt->params.len == 0 || CONST_STR_IS("0", vt->params.chars)) {
		/* Clear from cursor to end of screen */
		for (int col = vt->current.col; col < vt->cols; col++) {
			cell = vt_get_cell(buffer, vt->current.row, col);
			if (cell)
				cell->flags &= ~VT_FLAG_CELL_SET;
		}
		for (int row = vt->current.row + 1; row < vt->rows; row++) {
			for (int col = 0; col < vt->cols; col++) {
				cell = vt_get_cell(buffer, row, col);
				if (cell)
					cell->flags &= ~VT_FLAG_CELL_SET;
			}
		}
	} else if (vt->params.len > 0 && CONST_STR_IS("1", vt->params.chars)) {
		/* Clear screen from 0,0 for cursor */
		for (int row = 0; row < vt->current.row; row++) {
			for (int col = 0; col < vt->cols; col++) {
				cell = vt_get_cell(buffer, row, col);
				if (cell)
					cell->flags &= ~VT_FLAG_CELL_SET;
			}
		}
		for (int col = 0; col <= vt->current.col; col++) {
			cell = vt_get_cell(buffer, vt->current.row, col);
			if (cell)
				cell->flags &= ~VT_FLAG_CELL_SET;
		}
	} else if (vt->params.len > 0 && CONST_STR_IS("2", vt->params.chars)) {
		/* Clear entire screen */
		for (int row = 0; row < vt->rows; row++) {
			for (int col = 0; col < vt->cols; col++) {
				cell = vt_get_cell(buffer, row, col);
				if (cell)
					cell->flags &= ~VT_FLAG_CELL_SET;
			}
		}
	} else {
		/* Any other mode is an error. Do nothing */
		DLOG("Unsupported csi_clear_screen type '%s'", vt->params.chars);
	}

	vt->vt_mode = MODE_NORMAL;
}

static void csi_position_cursor(struct buffer *buffer, struct vt_cell *cell, char c) {
	struct vt *vt = &buffer->vt;
	int row;
	int col;
	int result;

	if (vt->params.len == 0 ||
	     (vt->params.len == 1 && CONST_STR_IS(";", vt->params.chars))) {
		row = 0;
		col = 0;
		result = 2; /* Match successful sscanf below */
	}

	if (vt->params.len >= 3) {
		result = sscanf(vt->params.chars, "%u;%u", &row, &col);
		if (result == 2) {
			/*
			 * The command is 1-indexed with 0 and 1 aliased where
			 * we are 0-indexed, convert.
			 * */
			if (row != 0)
				row--;
			if (col != 0)
				col--;
		}
	}

	if (result == 2) {
		vt->current.row = row;
		vt->current.col = col;
	}

	vt->vt_mode = MODE_NORMAL;
}

static void csi_move_cursor_up(struct buffer *buffer, struct vt_cell *cell, char c) {
	int distance;
	int result;
	struct vt *vt = &buffer->vt;

	if (vt->params.len == 0) {
		distance = 1;
		result = 1; /* Match successful sscanf below */
	} else {
		result = sscanf(vt->params.chars, "%u", &distance);
	}

	if (result == 1) {
		if (distance == 0)
			distance = 1;

		vt->current.row = max(0, vt->current.row - distance);
	}

	vt->vt_mode = MODE_NORMAL;
}

static void csi_move_cursor_down(struct buffer *buffer, struct vt_cell *cell, char c) {
	int distance;
	int result;
	struct vt *vt = &buffer->vt;

	if (vt->params.len == 0) {
		distance = 1;
		result = 1; /* Match successful sscanf below */
	} else {
		result = sscanf(vt->params.chars, "%u", &distance);
	}

	if (result == 1) {
		if (distance == 0)
			distance = 1;

		vt->current.row = min(vt->rows - 1, vt->current.row + distance);
	}

	vt->vt_mode = MODE_NORMAL;
}

static void csi_move_cursor_left(struct buffer *buffer, struct vt_cell *cell, char c) {
	int distance;
	int result;
	struct vt *vt = &buffer->vt;

	if (vt->params.len == 0) {
		distance = 1;
		result = 1; /* Match successful sscanf below */
	} else {
		result = sscanf(vt->params.chars, "%u", &distance);
	}

	if (result == 1) {
		if (distance == 0)
			distance = 1;

		vt->current.col = max(0, vt->current.col - distance);
	}

	vt->vt_mode = MODE_NORMAL;
}

static void csi_move_cursor_right(struct buffer *buffer, struct vt_cell *cell, char c) {
	int distance;
	int result;
	struct vt *vt = &buffer->vt;

	if (vt->params.len == 0) {
		distance = 1;
		result = 1; /* Match successful sscanf below */
	} else {
		result = sscanf(vt->params.chars, "%u", &distance);
	}

	if (result == 1) {
		if (distance == 0)
			distance = 1;

		vt->current.col = min(vt->cols - 1, vt->current.col + distance);
	}

	vt->vt_mode = MODE_NORMAL;
}

static void csi_clear_line(struct buffer *buffer, struct vt_cell *cell, char c) {
	struct vt *vt = &buffer->vt;

	if (vt->params.len == 0 || CONST_STR_IS("0", vt->params.chars)) {
		/* Clear from cursor to end of line */
		for (int col = vt->current.col; col < vt->cols; col++) {
			cell = vt_get_cell(buffer, vt->current.row, col);
			if (cell)
				cell->flags &= ~VT_FLAG_CELL_SET;
		}
	} else if (vt->params.len > 0 && CONST_STR_IS("1", vt->params.chars)) {
		/* Clear from start of line to cursor */
		for (int col = 0; col <= vt->current.col; col++) {
			cell = vt_get_cell(buffer, vt->current.row, col);
			if (cell)
				cell->flags &= ~VT_FLAG_CELL_SET;
		}
	} else if (vt->params.len > 0 && CONST_STR_IS("2", vt->params.chars)) {
		/* Clear entire line */
		for (int col = 0; col < vt->cols; col++) {
			cell = vt_get_cell(buffer, vt->current.row, col);
			if (cell)
				cell->flags &= ~VT_FLAG_CELL_SET;
		}
	} else {
		/* Any other mode is an error. Do nothing */
		DLOG("Unsupported csi_clear_line type '%s'", vt->params.chars);
	}
	vt->vt_mode = MODE_NORMAL;
}

static void csi_tabstop_clear(struct buffer *buffer, struct vt_cell *cell, char c) {
	struct vt *vt = &buffer->vt;

	if (vt->params.len == 0 || CONST_STR_IS("0", vt->params.chars)) {
		BITMAP_SETBIT(&vt->current.tabstops, vt->current.col, 0);
	} else if (CONST_STR_IS("3", vt->params.chars)) {
		for (int i = 0; i < MAX_COLUMNS; i++)
			BITMAP_SETBIT(&vt->current.tabstops, i, 0);
	} else {
		DLOG("Unsupported csi_tabstop_clear type '%s'", vt->params.chars);
	}
	vt->vt_mode = MODE_NORMAL;
}

static void decode_mode(struct vt *vt, char *mode, bool val) {
	DLOG("Unsupported mode '%s'", mode);
}

static void parse_mode(struct vt *vt, bool val) {
	char *cur; /* Start of the current option */
	char *next; /* Start of the next option */
	char *end; /* one past the end of the valid chars in param */

	end = vt->params.chars + vt->params.len;
	cur = vt->params.chars;

	while (cur < end) {
		while (next < end && *next != ';')
			next++;
		if (next < end) {
			*next = '\0'; /* change ; into nul */
			next++;
		}

		decode_mode(vt, cur, val);

		cur = next;
	}
}

static void csi_set_mode(struct buffer *buffer, struct vt_cell *cell, char c) {
	struct vt *vt = &buffer->vt;

	parse_mode(vt, true);

	vt->vt_mode = MODE_NORMAL;
}

static void csi_reset_mode(struct buffer *buffer, struct vt_cell *cell, char c) {
	struct vt *vt = &buffer->vt;

	parse_mode(vt, false);

	vt->vt_mode = MODE_NORMAL;
}

static void csi_special_graphics_mode(struct buffer *buffer, struct vt_cell *cell, char c) {
	struct vt *vt = &buffer->vt;
	char *str = vt->params.chars;
	char *end = vt->params.chars + vt->params.len;
	char *next;
	uint8_t attr;

	while (str < end) {
		next = str;
		while (next < end && *next != ';')
			next++;
		if (next < end) {
			*next = '\0'; /* change ; into nul */
			next++;
		}

		if (vt->params.len == 0 || CONST_STR_IS("0", str)) {
			vt->current.flags &= ~VT_ALL_STYLES;
		} else {
			if (sscanf(str, "%hhu", &attr) != 1) {
				DLOG("Invalid attribute '%s'", str);
				str++; /* Ensure forward progress and don't fall into an infinite loop */
				continue;
			}
 
			if (VT_ALL_STYLES & (1ULL << attr))
				vt->current.flags |= (1ULL << attr);
			else
				DLOG("Unknown graphics attribute '%s'", str);
		}

		str = next;
	}

	vt->vt_mode = MODE_NORMAL;
}

static void csi_mode(struct buffer *buffer, struct vt_cell *cell, char c) {
	switch (c) {
		DEFAULT(collect_params);
		HANDLE('A', csi_move_cursor_up);
		HANDLE('B', csi_move_cursor_down);
		HANDLE('C', csi_move_cursor_right);
		HANDLE('D', csi_move_cursor_left);
		HANDLE('J', csi_clear_screen);
		HANDLE('K', csi_clear_line);
		HANDLE('f', csi_position_cursor);
		HANDLE('g', csi_tabstop_clear);
		HANDLE('h', csi_set_mode);
		HANDLE('l', csi_reset_mode);
		HANDLE('m', csi_special_graphics_mode);
	}
}

static void osc_set_icon_name(struct vt *vt, char *args) {
	if (args)
		strncpy(vt->icon_name, args, sizeof(vt->icon_name) - 1);
	else
		vt->icon_name[0] = '\0';
}

static void osc_set_window_title(struct vt *vt, char *args) {
	if (args)
		strncpy(vt->window_title, args, sizeof(vt->window_title) - 1);
	else
		vt->window_title[0] = '\0';
}

static void osc_process(struct buffer *buffer, struct vt_cell *cell, char c) {
	/* OSC command have a free form format where the command starts with
	 * OSC and ends with a string terminator. The string terminator
	 * isn't in the parameters we have been given.
	 *
	 * Normally the command is an integer right at the beginning
	 * separated by semicolons from the arguments.
	 */
	struct vt *vt = &buffer->vt;
	char *args;
	char *cmd = &vt->params.chars[0];

	args = memchr(vt->params.chars, ';', vt->params.len);
	if (args) {
		/* separate the command from the arguments */
		*args = '\0';
		args++;

		if (args > cmd + vt->params.len)
			args = NULL;
	}

	if (CONST_STR_IS("0", cmd)) {
		osc_set_icon_name(vt, args);
		osc_set_window_title(vt, args);
	} else if (CONST_STR_IS("1", cmd)) {
		osc_set_icon_name(vt, args);
	} else if (CONST_STR_IS("2", cmd)) {
		osc_set_window_title(vt, args);
	} else {
		/* Any other command is an error. Do nothing */
		DLOG("Unsupported OSC command '%s' with args '%s'", cmd, args);
	}

	vt->vt_mode = MODE_NORMAL;
}

static void osc_process_trim(struct buffer *buffer, struct vt_cell *cell, char c) {
	/* A two byte string terminator was used, so we need to trim one
	 * byte before we process the OSC string.
	 */
	if (buffer->vt.params.len > 0) {
		buffer->vt.params.chars[buffer->vt.params.len - 1] = '\0';
		buffer->vt.params.len--;
	}
	osc_process(buffer, cell, c);
}

static void osc_mode(struct buffer *buffer, struct vt_cell *cell, char c) {
	switch (c) {
		DEFAULT(collect_params);
		HANDLE('\a', osc_process);
		HANDLE('\\', osc_process_trim);
	}
}

/*
 * This is the state change table for the terminal emulation. It is
 * basically a matrix of states and input bytes. This is a function which is
 * called with the terminal and the byte to be processed. This function then
 * performs whatever work is necessary for the emulation.
 *
 * Note: Order of modes must match enum vt_modes above.
 */
static const struct terminal_def terminal_emulation = {
	.max_mode = MODE_NUM,
	.modes = {
		normal_mode,
		escape_mode,
		csi_mode,
		osc_mode,
	},
};

void vt_interpret(struct buffer *buffer, char c) {
	struct vt_cell *cell;
	struct vt *vt = &buffer->vt;

	cell = vt_get_cell(buffer, vt->current.row, vt->current.col);
	terminal_emulation.modes[vt->vt_mode](buffer, cell, c);

	if (vt->current.col == vt->cols) {
		/* End of the line, move down one */
		DLOG("End of line reached");
		if (vt->flags & VT_FL_AUTOWRAP) {
			vt->current.col = 0;
			vt->current.row++;
		} else {
			vt->current.col = vt->cols - 1;
		}

	}

	if (vt->current.row == vt->rows) {
		/* Last line in the buffer, scroll */
		DLOG("End of buffer reached");
		vt->current.row--;
		if (vt->flags & VT_FL_AUTOSCROLL)
			vt_scroll_up(buffer);
	}
}
