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
#ifndef LOG_H
#define LOG_H

#include <stdio.h>

#include "options.h"

/*
 * Verbose log, logging that won't be useful to anybody but developers.
 */
#define VLOG(fmt, ...) do {                               \
	if (cmd_options.verbose >= 3)                     \
		fprintf(stderr, fmt "\n", ##__VA_ARGS__); \
} while(0)

/*
 * Debug log, logging which might be useful to people trying to figure out why something doesn't
 * work.
 */
#define DLOG(fmt, ...) do {                               \
	if (cmd_options.verbose >= 2)                     \
		fprintf(stderr, fmt "\n", ##__VA_ARGS__); \
} while (0)

/*
 * Warn log, logging something the user should know about which isn't normal.
 */
#define WLOG(fmt, ...) do {                               \
	if (cmd_options.verbose >= 1)                     \
		fprintf(stderr, fmt "\n", ##__VA_ARGS__); \
} while (0)

/*
 * Error log, logging the user must know about because something is broken.
 */
#define ELOG(fmt, ...) do {                               \
	if (cmd_options.verbose >= 0)                     \
		fprintf(stderr, fmt "\n", ##__VA_ARGS__); \
} while (0)

/*
 * Normal informational messages, often as a result of user input.
 */
#define NOTIFY(fmt, ...) do {                       \
	fprintf(stdout, fmt "\n", ##__VA_ARGS__); \
} while (0)

#endif
