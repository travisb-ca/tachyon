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
 * Header for buffers, which represent a psuedo-terminal which can be
 * multiplexed.
 */
#ifndef BUFFER_H
#define BUFFER_H

#include <stdint.h>

#include "loop.h"
#include "predictor.h"

#define BUFFER_BUF_SIZE 1024

struct buffer_cell {
	char c;
#define BUF_CELL_SET (1 << 0) /* This cell is in use */
	uint8_t flags;
};

struct buffer_line {
	struct buffer_line *next, *prev;
	uint16_t len;
	struct buffer_cell cells[0];
};

struct buffer {
	struct loop_fd fd;
	struct predictor predictor;

	int bufid;

	uint16_t current_row;
	uint16_t current_col;

	int buf_out_used;
	char buf_out[BUFFER_BUF_SIZE];

	uint16_t rows;
	uint16_t cols;

	struct buffer_line *topmost;
	struct buffer_line *bottommost;
	struct buffer_line **lines;
};

struct buffer *buffer_init(int bufid);
void buffer_free(struct buffer *buffer);
int buffer_set_winsize(struct buffer *buf, int rows, int cols);
int buffer_output(struct buffer *buffer, int size, char *buf);
int buffer_input(struct buffer *buffer, int size, char *buf);

#endif
