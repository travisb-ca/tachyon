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
#include <pwd.h>
#include <stdlib.h>
#include <string.h>

#include "loop.h"
#include "controller.h"
#include "util.h"
#include "log.h"
#include "tty.h"
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
			result = controller_output(result, bytes);
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

static char login_shell[128];

/*
 * Try hard to determine which shell the user uses at login. If that can't be
 * determined then /bin/sh is used.
 *
 * Returns:
 * - a valid static char*
 */
static char *get_login_shell(void) {
	int result;

	if (login_shell[0] != '\0')
		return login_shell;

	{
		char *username = NULL;
		struct passwd passwd;
		struct passwd *p_result = NULL;
		char buf[1024];

		username = getenv("USER");
		if (!username) {
			username = getlogin();
		}

		if (username) {
			result = getpwnam_r(username, &passwd, buf, sizeof(buf), &p_result);
			if (result)
				ELOG("getpwnam_r returned %d", result);
		}

		if (!p_result) {
			p_result = &passwd;
			passwd.pw_shell = "/bin/bash";
		}

		strncpy(login_shell, p_result->pw_shell, sizeof(login_shell));

		return login_shell;
	}
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
	char *login_shell;
	int result;

	login_shell = get_login_shell();

	buffer = malloc(sizeof(buffer));
	if (!buffer)
		return NULL;

	memset(buffer, 0, sizeof(*buffer));

	buffer->bufid = bufid;
	buffer->fd.poll_flags = POLLIN | POLLPRI;
	buffer->fd.poll_callback = buffer_cb;
	buffer->fd.fd = tty_new(login_shell);

	if (buffer->fd.fd < 0)
		goto out_free;

	result = loop_register(&buffer->fd);
	if (result != 0)
		goto out_free_fd;

	return buffer;

out_free_fd:
	close(buffer->fd.fd);

out_free:
	free(buffer);
	return NULL;
}

int buffer_set_winsize(struct buffer *buf, int rows, int cols) {
	return tty_set_winsize(buf->fd.fd, rows, cols);
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
	if (size > sizeof(buffer->buf_out) - buffer->buf_out_used)
		return EAGAIN;

	memcpy(&buffer->buf_out + buffer->buf_out_used,
	       buf, size);
	buffer->buf_out_used += size;
	buffer->fd.poll_flags |= POLLOUT;

	return 0;
}
