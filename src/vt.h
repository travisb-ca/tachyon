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
 * Header for the virtual terminal emulation functions
 */
#ifndef VT_H
#define VT_H

#include "util.h"
#include "config.h"

#define VT_STYLE_BOLD       (1 << 0)
#define VT_STYLE_UNDERSCORE (1 << 1)
#define VT_STYLE_BLINK      (1 << 2)
#define VT_STYLE_REVERSE    (1 << 3)

struct vt_cell {
	char c;
#define BUF_CELL_SET (1 << 0) /* This cell is in use */
	uint8_t flags;
	uint8_t style;
};

struct vt_line {
	struct vt_line *next, *prev;
	uint16_t len;
	struct vt_cell cells[0];
};

struct vt_cursor_mode {
	uint16_t row;
	uint16_t col;

	uint8_t style;

	BITMAP_DECLARE(MAX_COLUMNS) tabstops;
};

struct vt {
	int vt_mode;
	uint16_t rows;
	uint16_t cols;
	struct vt_cursor_mode current;
	struct vt_cursor_mode saved;

/* When reaching the end of line start at the beginning of the next line */
#define VT_FL_AUTOWRAP (1 << 0)
/* When reaching the bottom of the screen scroll all text up one line */
#define VT_FL_AUTOSCROLL (1 << 1)
	uint32_t flags;

	struct vt_line *topmost; /* Earliest line in the scroll buffer */
	struct vt_line *bottommost; /* Latest line in the scroll buffer */
	struct vt_line **lines; /* rows x cols view of writeable scroll buffer */

	/* Places to hold interm escape code parameters */
	struct {
		char chars[32];
		uint16_t len;
	} params;
};

int vt_init(struct vt *vt);
void vt_free(struct vt *vt);
void vt_interpret(struct buffer *buffer, char c);
struct vt_cell *vt_get_cell(struct buffer *buf, unsigned int row, unsigned int col);

#endif
