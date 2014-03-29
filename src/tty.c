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

#define _XOPEN_SOURCE 600

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
#include <ctype.h>

#include "log.h"
#include "util.h"
#include "options.h"

#include "tty.h"

/*
 * Create a new slave tty running the given command.
 *
 * command is the command to exec after creating the tty
 * bufnum is the number to assign to the TACHYON_BUFNUM environment variable
 *
 * Returns the master filedescriptor on success. On failure returns:
 * -1	Failed posix_openpt
 * -2	Failed grantpt
 */
int tty_new(char *command, int bufnum) {
	int result;
	int pty_master;
	int pty_slave;

	pty_master = posix_openpt(O_RDWR | O_NOCTTY);

	if (pty_master < 0) {
		ELOG("openpt failed %d %d", pty_master, errno);
		result = -1;
		goto err_master;
	}
	close_on_exec(pty_master);

	result = grantpt(pty_master);
	if (result != 0) {
		ELOG("granpt failed %d %d", result, errno);
		result = -2;
		goto err_master;
	}

	result = unlockpt(pty_master);
	if (result != 0) {
		ELOG("unlockpt failed %d %d", result, errno);
		result = -3;
		goto err_master;
	}

	pty_slave = open(ptsname(pty_master), O_RDWR);
	if (pty_slave < 0) {
		ELOG("failed to open slave %d %d", pty_slave, errno);
		result = -4;
		goto err_slave;
	}

	if (fork()) {
		/* Parent */
		close(pty_slave);
		return pty_master;
	} else {
		/* Child */
		char *cmd_name;
		int num_args = 1; /* cmd_name counts as an arg */
		char buf[1024];
		char **args;

		memcpy(buf, cmd_options.new_buf_command, sizeof(buf));

		/* First we need to count the number of arguments */
		for (int i = 1; i < sizeof(buf); i++) {
			if (buf[i] == '\0')
				break;

			if (isspace(buf[i-1]) && !isspace(buf[i]))
				num_args++;
		}
		DLOG("Shell command had %d args", num_args);

		/* Now that we know how many arguments we have we can create our array */
		args = calloc(num_args + 1, sizeof(char *));
		if (!args) {
			ELOG("Unable to allocate arg buffer");
			return 9;
		}

		cmd_name = strrchr(buf, '/');
		if (!cmd_name)
			cmd_name = "unknown";
		else
			cmd_name++; /* skip last '/' */

		args[0] = cmd_name;

		/*
		 * Now we fill in the pointers to the start of the args,
		 * making sure to terminate each arg with a nil.
		 */
		for (int i = 1, n = 1; i < sizeof(buf); i++) {
			if (isspace(buf[i]))
				buf[i] = '\0';

			if (buf[i - 1] == '\0' && buf[i] != '\0')
				args[n++] = &buf[i];
		}

		if (cmd_options.verbose >= 2)
			for (int i = 0; i < num_args + 1; i++)
				if (args[i])
					DLOG("arg %d is '%s'", i, args[i]);
				else
					DLOG("arg %d is NULL", i);

		/*
		 * Add the special environment variables
		 */
		{
			char bufnum_str[16];
			snprintf(bufnum_str, sizeof(bufnum_str), "%d", bufnum);
			/*
			 * If we fail here due to lack of memory we'll power on
			 * in the hopes that we can still exec below.
			 */
			setenv("TACHYON_BUFNUM", bufnum_str, 1);
			setenv("TACHYON_SESSION", cmd_options.session_name, 1);
		}

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
			ELOG("slave failed to set controlling tty %d %d", result, errno);
			return 7;
		}

		/* Use buf because the command path needs to be nil separated from the arguments */
		result = execv(buf, args);

		ELOG("slave failed to exec %d %d", result, errno);
		return 8;
	}

err_slave:
	close(pty_slave);

err_master:
	close(pty_master);

	return result;
}

static struct termios original_term_state;

void tty_save_termstate(void) {
	tcgetattr(0, &original_term_state);
}

void tty_restore_termstate(void) {
	tcsetattr(0, TCSANOW, &original_term_state);
}

int tty_configure_control_tty(void) {
	struct termios termstate;

	if (tcgetattr(0, &termstate))
		return -1;
	termstate.c_lflag &= ~(ICANON | ECHO | ECHONL);
	termstate.c_cc[VMIN] = 1;
	if (tcsetattr(0, TCSANOW, &termstate))
	    return -1;

	setvbuf(stdout, NULL, _IONBF, 0);

	return 0;
}

int tty_set_winsize(int fd, int rows, int cols) {
	int result;
	struct winsize size = {rows, cols, 0, 0};

	result = ioctl(fd, TIOCSWINSZ, &size);
	DLOG("Setting winsize to %d x %d: %d", rows, cols, result);

	return result;
}

struct winsize tty_get_winsize(int fd) {
	int result;
	struct winsize size;

	result = ioctl(fd, TIOCGWINSZ, &size);

	if (result != 0)
		WLOG("Unable to get window size %d", errno);

	return size;
}
