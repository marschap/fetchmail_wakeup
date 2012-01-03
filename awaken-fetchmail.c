/*
 * awaken-fetchmail: send SIGUSR1 to a fetchmail daemon in order to wake it
 *
 * Copyright (C) 2009-2012 Peter Marschall <peter@adpm.de>
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

#
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>		/* for isatty() */
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>		/* for strerror() */
#include <getopt.h>		/* for getopt_long() */
#include <syslog.h>		/* for openlog() */
#include <stdarg.h>		/* for vsyslog() */
#include <libgen.h>		/* for basename() */


#if !defined(FETCHMAIL_PIDFILE)
#  define FETCHMAIL_PIDFILE	"/var/run/fetchmail/fetchmail.pid"
#endif


/* declare functions */
void logger(int priority, const char *format, ...);
void usage(int status);


/* declare global variables */
static char *progname = NULL;
static int quiet = 0;
static int verbose = 0;


int main(int argc, char *argv[])
{
	const char shortopts[] = "qhv";
	const struct option longopts[] = {
		{ "quiet",   no_argument, NULL, 'q' },
		{ "verbose", no_argument, NULL, 'v' },
		{ "help",    no_argument, NULL, 'h' },
		{ NULL, 0, NULL, 0 }
	};

	int optc;
	FILE *file;

	progname = basename(argv[0]);

	/* parse options */
	while ((optc = getopt_long(argc, argv, shortopts, longopts, NULL)) != EOF) {
		switch (optc) {
			case 'h':
				usage(EXIT_SUCCESS);
				break;
			case 'q':
				quiet++;
				break;
			case 'v':
				verbose++;
				break;
			default:
				usage(EXIT_FAILURE);
		}
	}

	/* do not expect any non-option arguments */
	if (optind != argc)
		usage(EXIT_FAILURE);

	errno = 0;
	file = fopen(FETCHMAIL_PIDFILE, "r");

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
					if (verbose)
						logger(LOG_INFO, "sending signal SIGUSR1 to PID %ld", pid);
					fclose(file);
					exit(EXIT_SUCCESS);
				}
				else
					logger(LOG_ERR, "unable to send signal SIGUSR1 to PID %ld: %s",
						pid, strerror(errno));
			}
			else
				logger(LOG_ERR, "no numeric PID found in file '%s'", FETCHMAIL_PIDFILE);
		}
		else
			logger(LOG_ERR, "unable to read from file '%s': %s",
				FETCHMAIL_PIDFILE, strerror(errno));

		fclose(file);
	}
	else
		logger(LOG_ERR, "unable to open file '%s': %s",
			FETCHMAIL_PIDFILE, strerror(errno));

	exit(EXIT_FAILURE);
}


/* log to syslog */
void logger(int priority, const char *format, ...)
{
	static int opened = 0;

	if (!opened) {
		openlog(progname, 0, LOG_MAIL);
		opened++;
	}

	if (!quiet) {
		va_list ap;

		va_start(ap, format);
		vsyslog(priority | LOG_MAIL, format, ap);
		va_end(ap);
	}
}

/* display usage message */
void usage(int status)
{
	if (isatty(fileno(stderr))) {
		fprintf(stderr, "Usage:  %s [<options>]\n", progname);

		if (status == 0) {
			fprintf(stderr,
				"  where <options> are:\n"
				"    -h  --help                 show this help page\n"
				"    -q  --quiet                be quiet: suppress logging error messages\n"
				"    -v  --verbose              be verbose: log informational messages\n");
		}
		else
			fprintf(stderr, "For help, type: %s -h\n", progname);

	}

	exit(status);
}
