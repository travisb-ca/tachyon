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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <getopt.h>
#include <pwd.h>
#include <stdlib.h>

#include "tty.h"
#include "pal.h"
#include "log.h"
#include "loop.h"
#include "buffer.h"
#include "controller.h"
#include "options.h"

#define VERSION "0.01"
#define WELCOME_MSG "Tachyon v." VERSION

/* Default values for the options are set here */
struct cmd_options cmd_options = {
	.predict = false,
	.verbose = 1,
	.new_buf_command = "",
	.keys = {
		.meta= 't',
		.buffer_create = 'c',
		.buffer_next = 'n',
		.buffer_prev = 'p',
	},
};

const static struct option parameters[] = {
	{"help"    , no_argument       , NULL , 'h'}  , 
	{"hello"   , no_argument       , NULL , 'H'}  , 
	{"predict" , no_argument       , NULL , 'p'}  , 
	{"shell"   , required_argument , NULL , 's'}  , 
	{"verbose" , no_argument       , NULL , 'v'}  , 
	{"quiet"   , no_argument       , NULL , 'q'}  , 
	{NULL      , no_argument       , NULL , 0 }};

#define SHORTARGS "hHpqs:v"
static void usage(void)
{
	printf("tachyon [-hHpqv] [-s shell] \n");
	printf("	-h --help              - Display this message\n");
	printf("        -H --hello             - Display the version and welcome message on start\n");
	printf("	-p --predictor         - Turn on character prediction\n");
	printf("	-v --verbose           - increase log level (multiple allowed)\n");
	printf("	-s shell --shell=shell - command to run as shell for new buffer\n");
	printf("	-q --quiet             - decrease log level (multiple allowed)\n");
}

/*
 * Returns:
 * 0 - On success
 * 1 - When tachyon should terminate cleanly, such as after displaying the usage
 * 2 - When tachyon should terminate uncleanly, such as on parameter error
 */
static int process_args(int argn, char **args)
{
	int flag;

	while ((flag = getopt_long(argn, args, SHORTARGS, parameters, NULL)) != -1) {
		switch(flag) {
			case 'p':
				cmd_options.predict = true;
				break;

			case 'H':
				printf(WELCOME_MSG "\n");
				break;

			case 'v':
				cmd_options.verbose++;
				break;

			case 'q':
				cmd_options.verbose--;
				break;

			case 's':
				strncpy(cmd_options.new_buf_command, optarg,
					sizeof(cmd_options.new_buf_command));
				break;

			case 'h':
				usage();
				return 1;

			default:
				usage();
				return 2;
		}
	}

	return 0;
}

/*
 * Try hard to determine which shell the user uses at login. If that can't be
 * determined then /bin/sh is used.
 *
 * Returns:
 * 0 - given string now contains the login shell
 * 1 - Failure
 */
static int get_login_shell(char *destination, int size) {
	int result;

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

	strncpy(destination, p_result->pw_shell, size);

	return 0;
}

/*
 * Compute any dynamic default settings which weren't given to us by the user.
 * This is for things like session name, login shell and the like which we can't
 * know until runtime and that may/must differ between runs.
 *
 * Returns:
 * 0 - Success
 * 1 - Failure, settings in an undefined state
 */
static int set_defaults(void)
{
	int result;

	if (cmd_options.new_buf_command[0] == '\0') {
		result = get_login_shell(cmd_options.new_buf_command, sizeof(cmd_options.new_buf_command));
		if (result)
			return 1;
	}
	DLOG("Shell is '%s'", cmd_options.new_buf_command);

	return 0;
}

int main(int argn, char **args)
{
	int result;

	result = process_args(argn, args);
	if (result)
		return result - 1;

	if (set_defaults())
		return 1;

	tty_save_termstate();
	result = tty_configure_control_tty();
	DLOG("tty_configure_control_tty %d %d", result, errno);

	result = loop_init();
	DLOG("loop_init %d", result);

	result = controller_init();
	DLOG("register out %d", result);

	while (run) {
		if (!loop_run())
			ELOG("Running the loop failed");
	}

	tty_restore_termstate();

	return 0;
}
