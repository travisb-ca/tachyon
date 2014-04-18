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

/*
 * All the basic styles and flags a single character cell can have. The
 * flags and styles are interlaced to make use of otherwise unused style
 * bits while still keeping the mapping simple. This saves a couple of bytes
 * per cell.
 */
#define VT_FLAG_CELL_SET            (1ULL << 0) /* This cell is in use */
#define VT_STYLE_BOLD               (1ULL << 1)
#define VT_STYLE_UNDERSCORE         (1ULL << 4)
#define VT_STYLE_BLINK              (1ULL << 5)
#define VT_STYLE_REVERSE            (1ULL << 7)
#define VT_STYLE_FOREGROUND_BLACK   (1ULL << 30)
#define VT_STYLE_FOREGROUND_RED     (1ULL << 31)
#define VT_STYLE_FOREGROUND_GREEN   (1ULL << 32)
#define VT_STYLE_FOREGROUND_YELLOW  (1ULL << 33)
#define VT_STYLE_FOREGROUND_BLUE    (1ULL << 34)
#define VT_STYLE_FOREGROUND_MAGENTA (1ULL << 35)
#define VT_STYLE_FOREGROUND_CYAN    (1ULL << 36)
#define VT_STYLE_FOREGROUND_WHITE   (1ULL << 37)
#define VT_STYLE_BACKGROUND_BLACK   (1ULL << 40)
#define VT_STYLE_BACKGROUND_RED     (1ULL << 41)
#define VT_STYLE_BACKGROUND_GREEN   (1ULL << 42)
#define VT_STYLE_BACKGROUND_YELLOW  (1ULL << 43)
#define VT_STYLE_BACKGROUND_BLUE    (1ULL << 44)
#define VT_STYLE_BACKGROUND_MAGENTA (1ULL << 45)
#define VT_STYLE_BACKGROUND_CYAN    (1ULL << 46)
#define VT_STYLE_BACKGROUND_WHITE   (1ULL << 47)
#define VT_STYLE_MAX 48
 
#define VT_ALL_STYLES (              \
	VT_STYLE_BOLD               |\
	VT_STYLE_UNDERSCORE         |\
	VT_STYLE_BLINK              |\
	VT_STYLE_REVERSE            |\
	VT_STYLE_FOREGROUND_BLACK   |\
	VT_STYLE_FOREGROUND_RED     |\
	VT_STYLE_FOREGROUND_GREEN   |\
	VT_STYLE_FOREGROUND_YELLOW  |\
	VT_STYLE_FOREGROUND_BLUE    |\
	VT_STYLE_FOREGROUND_MAGENTA |\
	VT_STYLE_FOREGROUND_CYAN    |\
	VT_STYLE_FOREGROUND_WHITE   |\
	VT_STYLE_BACKGROUND_BLACK   |\
	VT_STYLE_BACKGROUND_RED     |\
	VT_STYLE_BACKGROUND_GREEN   |\
	VT_STYLE_BACKGROUND_YELLOW  |\
	VT_STYLE_BACKGROUND_BLUE    |\
	VT_STYLE_BACKGROUND_MAGENTA |\
	VT_STYLE_BACKGROUND_CYAN    |\
	VT_STYLE_BACKGROUND_WHITE   |\
        0                            \
)

struct vt_cell {
	char c;
	uint64_t flags;
};

struct vt_line {
	struct vt_line *next, *prev;
	uint16_t len;
	struct vt_cell cells[0];
};

struct vt_cursor_mode {
	uint16_t row;
	uint16_t col;

	uint64_t flags;

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

int vt_init(struct vt *vt, int rows, int cols);
void vt_free(struct vt *vt);
void vt_interpret(struct buffer *buffer, char c);
struct vt_cell *vt_get_cell(struct buffer *buf, unsigned int row, unsigned int col);

#endif
