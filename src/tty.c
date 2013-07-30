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

#include "log.h"

/*
 * Create a new slave tty running the given command.
 *
 * Returns the master filedescriptor on success. On failure returns:
 * -1	Failed posix_openpt
 * -2	Failed grantpt
 */
int tty_new(char *command)
{
	int result;
	int pty_master;
	int pty_slave;

	pty_master = posix_openpt(O_RDWR | O_NOCTTY);

	if (pty_master < 0) {
		ELOG("openpt failed %d %d\n", pty_master, errno);
		result = -1;
		goto err_master;
	}

	result = grantpt(pty_master);
	if (result != 0) {
		ELOG("granpt failed %d %d\n", result, errno);
		result = -2;
		goto err_master;
	}

	result = unlockpt(pty_master);
	if (result != 0) {
		ELOG("unlockpt failed %d %d\n", result, errno);
		result = -3;
		goto err_master;
	}

	pty_slave = open(ptsname(pty_master), O_RDWR);
	if (pty_slave < 0) {
		ELOG("failed to open slave %d %d\n", pty_slave, errno);
		result = -4;
		goto err_slave;
	}

	if (fork()) {
		/* Parent */
		close(pty_slave);
		return pty_master;
	} else {
		/* Child */
		struct termios term_settings;

		close(pty_master);

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
			ELOG("slave failed to set controlling tty %d %d\n", result, errno);
			return 7;
		}


		result = execv(command, NULL);

		ELOG("slave failed to exec %d %d\n", result, errno);
		return 8;
	}

err_slave:
	close(pty_slave);

err_master:
	close(pty_master);

	return result;
}

static struct termios original_term_state;

void tty_save_termstate(void)
{
	tcgetattr(0, &original_term_state);
}

void tty_restore_termstate(void)
{
	tcsetattr(0, TCSANOW, &original_term_state);
}

int tty_configure_control_tty(void)
{
	struct termios termstate;
	int i = 1;

	if (tcgetattr(0, &termstate))
		return -1;
	termstate.c_lflag &= ~(ICANON | ECHO | ECHONL);
	termstate.c_cc[VMIN] = 1;
	if (tcsetattr(0, TCSANOW, &termstate))
	    return -1;

	setvbuf(stdout, NULL, _IONBF, 0);

	fcntl(0, F_SETFL, &i);
}
