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
 * This file contains all the interpretations of terminal characters.
 */

#include "log.h"
#include "options.h"
#include "buffer.h"
#include "vt.h"

#define DEFAULT(func) \
	default: (func)(buffer, cell, c); break

#define HANDLE(ch, func) \
	case (ch): (func)(buffer, cell, c); break

enum {
	MODE_NORMAL,
	MODE_NUM
} vt_mode;

typedef void (*mode_fn)(struct buffer *buffer, struct buffer_cell *cell, char c);

struct terminal_def {
	int max_mode;
	mode_fn modes[];
};

static void ignore(struct buffer *buffer, struct buffer_cell *cell, char c) {}

static void normal_chars(struct buffer *buffer, struct buffer_cell *cell, char c)
{
	cell->c = c;
	cell->flags |= BUF_CELL_SET;
	buffer->vt.current_col++;
}

static void normal_backspace(struct buffer *buffer, struct buffer_cell *cell, char c)
{
	if (buffer->vt.current_col > 0)
		buffer->vt.current_col--;
}

static void normal_tab(struct buffer *buffer, struct buffer_cell *cell, char c)
{
	/*
	 * Emulate fixed tabstops. If the tabstop would move beyond the edge
	 * of the screen overwrite the last character and don't move.
	 */
	int tabstop;

	if (buffer->vt.current_col != buffer->vt.cols - 1)
		cell->c = '\t';

	tabstop = ((buffer->vt.current_col + 8) / 8) * 8;
	if (tabstop >= buffer->vt.cols)
		tabstop = buffer->vt.cols - 1;
	buffer->vt.current_col = tabstop;
}

static void normal_newline(struct buffer *buffer, struct buffer_cell *cell, char c)
{
	buffer->vt.current_row++;
}

static void normal_linefeed(struct buffer *buffer, struct buffer_cell *cell, char c)
{
	buffer->vt.current_col = 0;
}

static void normal_mode(struct buffer *buffer, struct buffer_cell *cell, char c)
{
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
		HANDLE(0x1b, ignore);
		HANDLE(0x1c, ignore);
		HANDLE(0x1d, ignore);
		HANDLE(0x1e, ignore);
		HANDLE(0x1f, ignore);
		HANDLE(0x7f, ignore);
	}
}

static const struct terminal_def terminal_dumb = {
	.max_mode = MODE_NUM,
	.modes = {
		normal_mode,
	},
};

void vt_interpret(struct buffer *buffer, char c)
{
	struct buffer_cell *cell;
	cell = buffer_get_cell(buffer, buffer->vt.current_row, buffer->vt.current_col);
	terminal_dumb.modes[MODE_NORMAL](buffer, cell, c);
}
