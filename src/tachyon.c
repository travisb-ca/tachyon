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
#include <unistd.h>

#include "tty.h"
#include "pal.h"
#include "log.h"
#include "loop.h"

#define NUM_FDS 3

#define STDIN 0
#define STDOUT 1
#define STDERR 2

#define SLAVE 2

struct slave_fd;

struct stdin_fd {
	struct loop_fd fd;

	int buf_in_used;
	char buf_in[1024];

	struct slave_fd *slave;
};

struct stdout_fd {
	struct loop_fd fd;

	int buf_out_used;
	char buf_out[1024];

	struct slave_fd *slave;
};

struct slave_fd {
	struct loop_fd fd;

	struct stdin_fd *in;
	struct stdout_fd *out;
};

static void stdin_cb(struct loop_fd *fd, int revents) {
	struct stdin_fd *in = (struct stdin_fd *)fd;
	int result;

	VLOG("STDIN is ready %d\n", revents);
	if (revents & (POLLHUP | POLLERR)) {
		WLOG("STDIN got error %d\n", revents);
		in->fd.poll_flags = 0;
	}

	if (revents & (POLLIN | POLLPRI)) {
		/* stdin -> slave */
		result = read(in->fd.fd, in->buf_in + in->buf_in_used, sizeof(in->buf_in) - in->buf_in_used);
		VLOG("read %d bytes from STDIN\n", result);
		if (result < 0) {
			WLOG("error reading stdin %d %d\n", result, errno);
		} else {
			in->buf_in_used += result;
			if (in->buf_in_used >= sizeof(in->buf_in)) {
				/* Buffer full, stop reading until we flush */
				DLOG("STDIN full, stopping reading\n");
				in->fd.poll_flags &= ~(POLLIN | POLLPRI);
			}

			/* Make sure we try and flush some data */
			in->slave->fd.poll_flags |= POLLOUT;
		}
	}
}

static void stdout_cb(struct loop_fd *fd, int revents) {
	struct stdout_fd *out = (struct stdout_fd *)fd;
	int result;

	VLOG("STDOUT is ready %d\n", revents);
	if (revents & (POLLHUP | POLLERR)) {
		WLOG("STDOUT got error %d\n", revents);
		out->fd.poll_flags = 0;
	}

	if (revents & POLLOUT) {
		/* flush data from slave */
		result = write(out->fd.fd, out->buf_out, out->buf_out_used);
		VLOG("wrote %d bytes from stdout\n", result);
		if (result <= 0) {
			WLOG("error writing stdout %d %d\n", result, errno);
		} else {
			out->buf_out_used -= result;
			if (out->buf_out_used == 0)
				out->fd.poll_flags = 0;

			/* There is room now, so accept more input from the slave */
			out->slave->fd.poll_flags |= POLLIN | POLLPRI;
		}
	}
}

static void slave_cb(struct loop_fd *fd, int revents) {
	struct slave_fd *slave = (struct slave_fd *)fd;
	int result;

	VLOG("SLAVE %d %d\n", slave->fd.poll_flags, revents);
	if (revents & (POLLHUP | POLLERR)) {
		WLOG("SLAVE got error %d\n", revents);
		slave->fd.poll_flags = 0;
	}

	if (revents & (POLLIN | POLLPRI)) {
		/* slave -> stdout */
		result = read(slave->fd.fd, slave->out->buf_out + slave->out->buf_out_used, sizeof(slave->out->buf_out) - slave->out->buf_out_used);
		VLOG("read %d bytes from SLAVE\n", result);
		if (result < 0) {
			WLOG("error reading slave %d %d\n", result, errno);
		} else {
			slave->out->buf_out_used += result;
			if (slave->out->buf_out_used >= sizeof(slave->out->buf_out)) {
				/* Buffer full, stop reading until we flush */
				slave->fd.poll_flags &= ~(POLLIN | POLLPRI);
			}

			/* Make sure we try and flush some data */
			slave->out->fd.poll_flags |= POLLOUT;
		}
	}

	if (revents & POLLOUT) {
		/* flush data from stdin */
		result = write(slave->fd.fd, slave->in->buf_in, slave->in->buf_in_used);
		VLOG("wrote %d bytes from SLAVE\n", result);
		if (result <= 0) {
			WLOG("error writing slave %d %d\n", result, errno);
		} else {
			slave->in->buf_in_used -= result;
			if (slave->in->buf_in_used == 0)
				slave->fd.poll_flags &= ~POLLOUT;

			/* There is room now, so accept more input from the stdin */
			slave->in->fd.poll_flags |= POLLIN | POLLPRI;
		}
	}
}

int main(int argn, char **args)
{
	int result;

	struct stdin_fd in = {
		.fd = {
			.fd = STDIN,
			.poll_flags = POLLIN | POLLPRI,
			.poll_callback = stdin_cb,
		},
		.buf_in_used = 0,
		.buf_in = {0},
		.slave = NULL,
	};

	struct stdout_fd out = {
		.fd = {
			.fd = STDOUT,
			.poll_flags = 0,
			.poll_callback = stdout_cb,
		},
		.buf_out_used = 0,
		.buf_out = {0},
		.slave = NULL,
	};

	struct slave_fd slave = {
		.fd = {
			.fd = 0,
			.poll_flags = POLLIN | POLLPRI,
			.poll_callback = slave_cb,
		},
	};

	result = tty_new("/bin/bash");
	if (result < 0) {
		ELOG("Failed to create slave %d\n", result);
		return 1;
	}

	slave.fd.fd = result;
	slave.in = &in;
	slave.out = &out;
	in.slave = &slave;
	out.slave = &slave;

	tty_save_termstate();
	result = tty_configure_control_tty();
	DLOG("tty_configure_control_tty %d %d\n", result, errno);

	result = loop_register((struct loop_fd *)&in);
	DLOG("register in %d\n", result);
	result = loop_register((struct loop_fd *)&out);
	DLOG("register out %d\n", result);
	result = loop_register((struct loop_fd *)&slave);
	DLOG("register slave %d\n", result);

	for (;;) {
		if (!loop_run())
			ELOG("Running the loop failed\n");
	}

	tty_restore_termstate();

	return 0;
}
