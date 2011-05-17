/* 
 * awaken-fetchmail: send SIGUSR1 to a fetchmail daemon in order to wake it
 *
 * Copyright (C) 2009-2011 Peter Marschall <peter@adpm.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License (version 2 or higher)
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 */


#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>

#if !defined(FETCHMAIL_PIDFILE)
#  define FETCHMAIL_PIDFILE	"/var/run/fetchmail/fetchmail.pid"
#endif

int main(int argc, char *argv[])
{
	FILE *file = fopen(FETCHMAIL_PIDFILE, "r");

	if (file != NULL) {
		char buffer[256];

		if (fread(buffer, 1, sizeof(buffer), file) > 0) {
			long pid;

			/* make sure buffer is '\0'-terminated */
			buffer[sizeof(buffer)-1] = '\0';

			/* convert buffer to pid (with error checking) */
			errno = 0;
			pid = strtol(buffer, NULL, 10);
	
			if ((pid > 0) && (errno == 0))  {
				if (kill(pid, SIGUSR1) == 0) {
					fclose(file);
					exit(EXIT_SUCCESS);
				}
			}
		}
		fclose(file);
	}
	exit(EXIT_FAILURE);
}

