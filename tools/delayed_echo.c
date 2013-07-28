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
 * A simple program which takes every character from stdin and echos it to stdout
 * after a delay.
 */
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <termios.h>

#define DELAY_US (2000 * 1000)
#define BUF_SIZE 4096

#define STDIN 0
#define STDOUT 1

int main(int argn, char **args)
{
	int result;
	struct timeval wait;
	struct timeval now;
	struct timeval *departure;
	fd_set fds;
	int used = 0;
	struct termios termstate;

	struct {
		char c;
		struct timeval departure;
	} buf[BUF_SIZE];

	tcgetattr(STDIN, &termstate);
	termstate.c_lflag &= ~(ICANON | ECHO | ECHONL);
	termstate.c_cc[VMIN] = 1;
	tcsetattr(STDIN, TCSANOW, &termstate);

	setvbuf(stdout, NULL, _IONBF, 0);

	used = 1;
	result = fcntl(STDIN, F_SETFL, &used);
	used = 0;

	for (;;) {
		if (used == 0) {
			wait.tv_sec = 10000000;
			wait.tv_usec = 0;
		} else {
			gettimeofday(&now, NULL);
			departure = &buf[0].departure;
			
			wait.tv_sec = departure->tv_sec - now.tv_sec;
			wait.tv_usec = departure->tv_usec - now.tv_usec;

			if (wait.tv_usec < 0) {
				wait.tv_sec -= 1;
				wait.tv_usec += 1000000;
			}
		}

		if (wait.tv_sec != 0 || wait.tv_usec != 0) {
			FD_ZERO(&fds);
			FD_SET(STDIN, &fds);
			result = select(STDIN + 1, &fds, NULL, NULL, &wait);
		} else {
			/* Print out the next character now */
			result = 0;
		}

		if (result == 0) {
			/* select timed out, output this character */
			putc(buf[0].c, stdout);
			used--;
			memmove(buf, buf + 1, sizeof(buf[0]) * used);
		} else {
			/* Have a new character */
			if (used == BUF_SIZE)
				continue; /* Drop it */

			result = read(STDIN, &(buf[used].c), 1);
			if (result != 1)
				continue;
			departure = &buf[used].departure;
			gettimeofday(departure, NULL);
			departure->tv_usec += DELAY_US;
			while (departure->tv_usec >   1000000) {
				departure->tv_usec -= 1000000;
				departure->tv_sec += 1;
			}
			used++;
		}
	}

	return 0;
}
