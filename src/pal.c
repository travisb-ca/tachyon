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
 * A series of wrappers for system facilities which are either non-existent or
 * insufficient on the given platform.
 */

#include <stdlib.h>
#include <poll.h>
#include <sys/select.h>

#include "pal.h"

#if defined(__APPLE__)

/*
 * poll() on Darwin doesn't support devices, which include pseudo-ttys. This
 * makes the Darwin poll() pretty useless for tachyon. The interface is
 * reasonably nice so we'll inefficiently emulate it here.
 */
int pal_poll(struct pollfd fds[], nfds_t nfds, int timeout)
{
	fd_set read_fds;
	fd_set write_fds;
	fd_set error_fds;
	int result;
	struct timeval wait_;
	struct timeval *wait = NULL;
	int i;
	int maxfd = -1;

	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);
	FD_ZERO(&error_fds);

	if (timeout != -1) {
		wait = &wait_;
		wait->tv_sec = timeout / 1000;
		wait->tv_usec = (timeout % 1000) * 1000;
	}

	for (i = 0; i < nfds; i++) {
		if (fds[i].events & (POLLIN | POLLPRI | POLLRDBAND))
			FD_SET(fds[i].fd, &read_fds);
		if (fds[i].events & (POLLOUT | POLLWRBAND))
			FD_SET(fds[i].fd, &write_fds);

		FD_SET(fds[i].fd, &error_fds);

		if (fds[i].fd > maxfd)
			maxfd = fds[i].fd;
	}

	result = select(maxfd + 1, &read_fds, &write_fds, &error_fds, wait);
	if (result < 0)
		goto out;

	for (i = 0; i < nfds; i++) {
		fds[i].revents = 0;

		if (FD_ISSET(fds[i].fd, &read_fds))
			fds[i].revents |= fds[i].events & (POLLIN | POLLPRI | POLLRDBAND);

		if (FD_ISSET(fds[i].fd, &write_fds))
			fds[i].revents |= fds[i].events & (POLLOUT | POLLWRBAND);

		if (FD_ISSET(fds[i].fd, &error_fds)) {
			fds[i].revents |= POLLERR | POLLHUP;
			fds[i].revents &= ~(POLLOUT | POLLWRBAND);
		}
	}

out:
	return result;
}

#else

int pal_poll(struct pollfd fds[], nfds_t nfds, int timeout)
{
	return poll(fds, nfds, timeout);
}

#endif
