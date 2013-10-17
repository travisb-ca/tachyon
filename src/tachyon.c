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

#include "tty.h"
#include "pal.h"
#include "log.h"
#include "loop.h"
#include "buffer.h"
#include "controller.h"
#include "options.h"

/* Default values for the options are set here */
struct cmd_options cmd_options = {
	.predict = false,
};

const static struct option parameters[] = {
	{"help"    , no_argument , NULL , 'h'}  , 
	{"predict" , no_argument , NULL , 'p'}  , 
	{NULL      , no_argument , NULL , 0 }};

static void usage(void)
{
	printf("tachyon -hp\n");
	printf("	-h --help      - Display this message\n");
	printf("	-p --predictor - Turn on character prediction\n");
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

	while ((flag = getopt_long(argn, args, "hp", parameters, NULL)) != -1) {
		switch(flag) {
			case 'p':
				cmd_options.predict = true;
				break;

			default:
			case 'h':
				usage();
				return 1;
		}
	}

	return 0;
}

int main(int argn, char **args)
{
	int result;

	result = process_args(argn, args);
	if (result)
		return result - 1;

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
