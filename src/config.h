/*
 * Copyright (C) 2014  Travis Brown (travisb@travisbrown.ca)
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
 * Compile time configuration options for Tachyon
 */

#ifndef CONFIG_H
#define CONFIG_H

/*
 * This is the maximum number of columns supported by Tachyon.
 */
#define MAX_COLUMNS 512

/*
 * Size of the buffer between the buffer and the controlling terminal.
 */
#define CONTROLLER_BUF_SIZE 102400

/*
 * Compile time limit on the number of buffers supported.
 */
#define CONTROLLER_MAX_BUFS 10

#endif
