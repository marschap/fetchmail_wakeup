/*
 * Fetchmail notification IMAP plugin for Dovecot
 *
 * Copyright (C) 2007 Guillaume Chazarain <guichaz@yahoo.fr>
 * - original version named wake_up_fetchmail.c
 *
 * Copyright (C) 2009-2011 Peter Marschall <peter@adpm.de>
 * - adaptions to dovecot 1.1, 1.2 & 2.0
 * - rename to fetchmail_wakeup.c
 * - configuration via dovecot.config
 *
 * License: LGPL v2.1
 *
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include "lib.h"
#include "imap-client.h"


#define FETCHMAIL_PIDFILE	"/var/run/fetchmail/fetchmail.pid"
#define FETCHMAIL_INTERVAL	60


/*
 * Dovecot's real IMAPv4 "IDLE" function
 */
static struct command *orig_cmd_idle_ptr;
static struct command orig_cmd_idle;
/*
 * Dovecot's real IMAPv4 "STATUS" function
 */
static struct command *orig_cmd_status_ptr;
static struct command orig_cmd_status;


/*
 * Don't bother waking up fetchmail too often
 */
static bool ratelimit(long interval)
{
	static struct timeval last_one;
	struct timeval now;
	long millisec_delta;

	if (gettimeofday(&now, NULL))
		return FALSE;

	millisec_delta = ((now.tv_sec - last_one.tv_sec) * 1000000 +
	                  now.tv_usec - last_one.tv_usec) / 1000;
	if (millisec_delta > interval * 1000) {
		last_one = now;
		return FALSE;
	}

	return TRUE;
}


/*
 * Send a signal to fetchmail or call a helper to awaken fetchmail
 */
static void fetchmail_wakeup(struct client_command_context *cmd)
{
	struct client *client = cmd->client;
	long fetchmail_interval = FETCHMAIL_INTERVAL;

	/* read config variables depending on the session */
	const char *fetchmail_helper = mail_user_plugin_getenv(client->user, "fetchmail_helper");
	const char *fetchmail_pidfile = mail_user_plugin_getenv(client->user, "fetchmail_pidfile");
	const char *interval_str = mail_user_plugin_getenv(client->user, "fetchmail_interval");

	/* convert config variable "fetchmail_interval" into a number */
	if (interval_str != NULL) {
		if (str_is_numeric(interval_str, '\0')) {
			long value = strtol(interval_str, NULL, 10);

			if (value > 0)
				fetchmail_interval = value;
			else
				i_warning("fetchmail_wakeup: fetchmail_interval must be a positive number");
		}
		else {
			i_warning("fetchmail_wakeup: fetchmail_interval must be a positive number");
		}
	}

	if (ratelimit(fetchmail_interval))
		return;

	/* if a helper application is defined, then call it */
	if ((fetchmail_helper != NULL) && (*fetchmail_helper != '\0')) {
		pid_t pid;
		int status;
		char *const *argv;

		switch (pid = fork()) {
			case -1:	// fork failed
				i_warning("fetchmail_wakeup: fork() failed");
				return;
			case 0:		// child
				argv = (char *const *) t_strsplit_spaces(fetchmail_helper, " ");
				if ((argv != NULL) && (*argv != NULL)) {
					execv(argv[0], argv);
					i_warning("fetchmail_wakeup: execv(%s) failed: %s",
						argv[0], strerror(errno));
					exit(1);
				}
				else {
					i_warning("fetchmail_wakeup: illegal fetchmail_helper");
					exit(1);
				}
			default:	// parent
				waitpid(pid, &status, 0);
		}
	}
	/* otherwise if a pid file name is given, signal fetchmail with that pid */
	else if ((fetchmail_pidfile != NULL) && (*fetchmail_pidfile != '\0')) {
		FILE *pidfile = fopen(fetchmail_pidfile, "r");
		if (pidfile) {
			pid_t pid = 0;

			if ((fscanf(pidfile, "%d", &pid) == 1) && (pid > 1))
				kill(pid, SIGUSR1);
			else
				i_warning("fetchmail_wakeup: error reading valid pid from %s",
					fetchmail_pidfile);
			fclose(pidfile);
		}
		else {
			i_warning("fetchmail_wakeup: error opening %s",
				 fetchmail_pidfile);
		}
	}
	/* otherwise warn on missing configuration */
	else {
		i_warning("fetchmail_wakeup: neither fetchmail_pidfile nor fetchmail_helper given");
	}
}


/*
 * Our IMAPv4 "IDLE" wrapper
 */
static bool new_cmd_idle(struct client_command_context *cmd)
{
	/* try to wake up fetchmail */
	fetchmail_wakeup(cmd);

	/* daisy chaining: call original IMAPv4 "IDLE" command chandler */
	return orig_cmd_idle.func(cmd);
}

/*
 * Our IMAPv4 "STATUS" wrapper
 */
static bool new_cmd_status(struct client_command_context *cmd)
{
	/* try to wake up fetchmail */
	fetchmail_wakeup(cmd);

	/* daisy chaining: call original IMAPv4 "STATUS" command chandler */
	return orig_cmd_status.func(cmd);
}


/*
 * Plugin init: remember dovecot's "IDLE" and "STATUS" functions and add our own
 * in place
 */
void fetchmail_wakeup_plugin_init(struct module *module)
{
	/* replace IMAPv4 "IDLE" command handler by our own */
	orig_cmd_idle_ptr = command_find("IDLE");
	if (orig_cmd_idle_ptr)
		memcpy(&orig_cmd_idle, orig_cmd_idle_ptr, sizeof(struct command));
	command_unregister("IDLE");
	command_register("IDLE", new_cmd_idle, orig_cmd_idle.flags);

	/* replace IMAPv4 "STATUS" command handler by our own */
	orig_cmd_status_ptr = command_find("STATUS");
	if (orig_cmd_status_ptr)
		memcpy(&orig_cmd_status, orig_cmd_status_ptr, sizeof(struct command));
	command_unregister("STATUS");
	command_register("STATUS", new_cmd_status, orig_cmd_status.flags);
}

/*
 * Plugin deinit: restore dovecot's "IDLE" and "STATUS" functions
 * The command name is dupped, for its memory location to be accessible even
 * when the plugin is unloaded
 */
void fetchmail_wakeup_plugin_deinit(void)
{
	/* restore previous IMAPv4 "IDLE" command handler */
	if (orig_cmd_idle_ptr) {
		command_unregister("IDLE");
		command_register(orig_cmd_idle.name, orig_cmd_idle.func, orig_cmd_idle.flags);
	}

	/* restore previous IMAPv4 "STATUS" command handler */
	if (orig_cmd_status_ptr) {
		command_unregister("STATUS");
		command_register(orig_cmd_status.name, orig_cmd_status.func, orig_cmd_status.flags);
	}
}


/*
 * declare dependency on IMAP
 */
const char fetchmail_wakeup_plugin_binary_dependency[] = "imap";

/* EOF */
