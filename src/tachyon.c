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
 * main() and argument processing of Tachyon.
 */

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "tty.h"

#define NUM_FDS 3

#define STDIN 0
#define STDOUT 1
#define STDERR 2

#define SLAVE 2

int main(int argn, char **args)
{
	int result;
	struct pollfd fds[NUM_FDS];
	char buf_in[1024];
	int buf_in_used = 0;
	char buf_out[1024];
	int buf_out_used = 0;

	memset(&fds, 0, sizeof(fds));

	fds[STDIN].fd = STDIN;
	fds[STDIN].events = POLLIN | POLLPRI;

	fds[STDOUT].fd = STDOUT;
	fds[STDOUT].events = 0;

	result = tty_new("/bin/bash");
	if (result < 0) {
		fprintf(stderr, "Failed to create slave %d\n", result);
		return 1;
	}

	fds[SLAVE].fd = result;
	fds[SLAVE].events = POLLIN | POLLPRI;

	for (;;) {
		result = poll(fds, NUM_FDS, -1);
		if (result <= 0) {
			fprintf(stderr, "poll failed %d %d\n", result, errno);
			continue;
		}
		fprintf(stderr, "%d fds ready\n", result);

		if (fds[STDIN].revents) {
			fprintf(stderr, "STDIN is ready\n");
			if (fds[STDIN].revents & (POLLHUP | POLLERR)) {
				fprintf(stderr, "STDIN got error %d\n", fds[STDIN].revents);
				fds[STDIN].events = 0;
			}

			if (fds[STDIN].revents & (POLLIN | POLLPRI)) {
				/* stdin -> slave */
				result = read(STDIN, buf_in + buf_in_used, sizeof(buf_in) - buf_in_used);
				fprintf(stderr, "read %d bytes from STDIN\n", result);
				if (result < 0) {
					fprintf(stderr, "error reading stdin %d %d\n", result, errno);
				} else {
					buf_in_used += result;
					if (buf_in_used >= sizeof(buf_in)) {
						/* Buffer full, stop reading until we flush */
						fprintf(stderr, "STDIN full, stopping reading\n");
						fds[STDIN].events &= ~(POLLIN | POLLPRI);
					}

					/* Make sure we try and flush some data */
					fds[SLAVE].events |= POLLOUT;
				}
			}
		}

		if (fds[STDOUT].revents) {
			fprintf(stderr, "STDOUT is ready\n");
			if (fds[STDOUT].revents & (POLLHUP | POLLERR)) {
				fprintf(stderr, "STDOUT got error %d\n", fds[STDOUT].revents);
				fds[STDOUT].events = 0;
			}

			if (fds[STDOUT].revents & POLLOUT) {
				/* flush data from slave */
				result = write(STDOUT, buf_out, buf_out_used);
				if (result <= 0) {
					fprintf(stderr, "error writing stdout %d %d\n", result, errno);
				} else {
					buf_out_used -= result;

					/* There is room now, so accept more input from the slave */
					fds[SLAVE].events |= POLLIN | POLLPRI;
				}
			}
		}
		fprintf(stderr, "SLAVE %d %d\n", fds[SLAVE].events, fds[SLAVE].revents);
		if (fds[SLAVE].revents) {
			if (fds[SLAVE].revents & (POLLHUP | POLLERR)) {
				fprintf(stderr, "SLAVE got error %d\n", fds[SLAVE].revents);
				fds[SLAVE].events = 0;
			}

			if (fds[SLAVE].revents & (POLLIN | POLLPRI)) {
				/* slave -> stdout */
				result = read(fds[SLAVE].fd, buf_out + buf_out_used, sizeof(buf_out) - buf_out_used);
				if (result < 0) {
					fprintf(stderr, "error reading slave %d %d\n", result, errno);
				} else {
					buf_out_used += result;
					if (buf_out_used >= sizeof(buf_out)) {
						/* Buffer full, stop reading until we flush */
						fds[SLAVE].events &= ~(POLLIN | POLLPRI);
					}

					/* Make sure we try and flush some data */
					fds[STDOUT].events |= POLL_OUT;
				}
			}

			if (fds[SLAVE].revents & POLLOUT) {
				/* flush data from stdin */
				result = write(SLAVE, buf_in, buf_in_used);
				if (result <= 0) {
					fprintf(stderr, "error writing slave %d %d\n", result, errno);
				} else {
					buf_in_used -= result;

					/* There is room now, so accept more input from the stdin */
					fds[STDIN].events |= POLLIN | POLLPRI;
				}
			}

		}
	}

	return 0;
}
