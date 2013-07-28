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
 * The tty interface and control portion of Tachyon.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/ioctl.h>

int main(int argn, char **args)
{
	int result;
	int pty_master;
	int pty_slave;
	char slave_path[4096];

	pty_master = posix_openpt(O_RDWR | O_NOCTTY);

	if (pty_master < 0) {
		fprintf(stderr, "openpt failed %d %d\n", pty_master, errno);
		return 1;
	}

	result = grantpt(pty_master);
	if (result != 0) {
		fprintf(stderr, "granpt failed %d %d\n", result, errno);
		return 2;
	}

	strncpy(slave_path, ptsname(pty_master), sizeof(slave_path));
	fprintf(stderr, "Slave path '%s'\n", slave_path);

	result = unlockpt(pty_master);
	if (result != 0) {
		fprintf(stderr, "unlockpt failed %d %d\n", result, errno);
		return 3;
	}

	pty_slave = open(slave_path, O_RDWR);
	if (pty_slave < 0) {
		fprintf(stderr, "failed to open slave %d %d\n", pty_slave, errno);
		return 4;
	}

	if (fork()) {
		/* Parent */
		fd_set fds_in;
		fd_set fds_out;
		char buf[1024];

		close(pty_slave);

		for (;;) {
			FD_ZERO(&fds_in);
			FD_SET(0, &fds_in);
			FD_SET(pty_master, &fds_in);

			result = select(pty_master + 1, &fds_in, NULL, NULL, NULL);
			if (result <= 0) {
				fprintf(stderr, "select finished, none ready %d %d\n", result, errno);
				continue;
			}

			if (FD_ISSET(0, &fds_in)) {
				/* stdin -> slave */
				result = read(0, buf, sizeof(buf));
				if (result > 0) {
					result = write(pty_master, buf, result);

					if (result <= 0)
						fprintf(stderr, "Failed to write to pty_master %d %d\n", result, errno);
				} else {
					fprintf(stderr, "Failed to read from stdin %d %d\n", result, errno);
				}
			}

			if (FD_ISSET(pty_master, &fds_in)) {
				/* slave -> stdout */
				result = read(pty_master, buf, sizeof(buf));
				if (result > 0) {
					result = write(1, buf, result);

					if (result <= 0)
						fprintf(stderr, "Failed to write to stdout %d %d\n", result, errno);
				} else {
					fprintf(stderr, "Failed to read from stdin %d %d\n", result, errno);
				}
			}
		}

	} else {
		/* Child */
		struct termios term_settings;

		close(pty_master);

		result = tcgetattr(pty_slave, &term_settings);
		if (result != 0) {
			fprintf(stderr, "slave failed to get term_settings %d %d\n", result, errno);
			return 5;
		}

		cfmakeraw(&term_settings);
		result = tcsetattr(pty_slave, TCSANOW, &term_settings);
		if (result != 0) {
			fprintf(stderr, "slave failed to change to raw %d %d\n", result, errno);
			return 6;
		}

		close(0);
		close(1);
		close(2);

		dup(pty_slave); /* replace stdin */
		dup(pty_slave); /* replace stdout */
		dup(pty_slave); /* replace stderr */

		close(pty_slave);

		setsid();

		result = ioctl(0, TIOCSCTTY, 1);
		if (result != 0) {
			fprintf(stderr, "slave failed to set controlling tty %d %d\n", result, errno);
			return 7;
		}

		result = execv("/bin/bash", NULL);

		fprintf(stderr, "slave failed to exec %d %d\n", result, errno);
		return 8;
	}

	return 0;
}
