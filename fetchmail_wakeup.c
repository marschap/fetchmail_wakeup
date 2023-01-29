/*
 * Fetchmail notification IMAP plugin for Dovecot
 *
 * Copyright (C) 2007 Guillaume Chazarain <guichaz@yahoo.fr>
 * - original version named wake_up_fetchmail.c
 *
 * Copyright (C) 2009-2023 Peter Marschall <peter@adpm.de>
 * - adaptions to dovecot 1.1, 1.2 [both deprecated], and 2.x
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
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include "lib.h"
#include "imap-client.h"


/* check that we have the minimal dovecot version required for compilation */
#if defined(DOVECOT_VERSION_MAJOR) && defined(DOVECOT_VERSION_MINOR)
#	if ((DOVECOT_VERSION_MAJOR << 24) + (DOVECOT_VERSION_MINOR << 16) <= 0x02010000)
#		error *** dovecot versioni too low: must be 2.1.0 or higher ***
#	endif
#else
#	error *** dovecot version unknown: must be 2.1.0 or higher ***
#endif


#define FETCHMAIL_INTERVAL	0


/*
 * make sure we have the right ABI version at runtime
 */
const char *fetchmail_wakeup_plugin_version = DOVECOT_ABI_VERSION;


/* commands that can be intercepted */
static const char *default_cmds[] = {
	"IDLE",
	"NOOP",
	"STATUS",
	"NOTIFY",
	NULL
};

/*
 * Get a interval value from config and parse it into a number (with fallback for failures)
 */
static long getenv_interval(struct mail_user *user, const char *name, long fallback)
{
	if (name != NULL) {
		const char *value_as_str = mail_user_plugin_getenv(user, name);

		if (value_as_str != NULL) {
			long value;

			if ((str_to_long(value_as_str, &value) < 0) || (value < 0)) {
				i_warning("fetchmail_wakeup: %s must be a positive number", name);
				return fallback;
			}
			else
				return value;
		}
	}
	return fallback;
}


/*
 * Don't bother waking up fetchmail too often
 */
static bool ratelimit(long interval)
{
	static struct timeval last_one;
	struct timeval now;
	long long millisec_delta;

	if (gettimeofday(&now, NULL))
		return FALSE;

	millisec_delta = ((now.tv_sec - last_one.tv_sec) * 1000000LL +
	                  now.tv_usec - last_one.tv_usec) / 1000LL;
	if (millisec_delta > interval * 1000LL) {
		last_one = now;
		return FALSE;
	}

	return TRUE;
}


/*
 * Send a signal to fetchmail or call a helper to awaken fetchmail
 */
static void fetchmail_wakeup(struct client_command_context *ctx)
{
	struct mail_user *user = ctx->client->user;	/* != NULL as checked by caller */
	long fetchmail_interval = FETCHMAIL_INTERVAL;

	/* read config variables depending on the session */
	const char *fetchmail_helper = mail_user_plugin_getenv(user, "fetchmail_helper");
	const char *fetchmail_pidfile = mail_user_plugin_getenv(user, "fetchmail_pidfile");

	/* convert config variable "fetchmail_interval" into a number */
	fetchmail_interval = getenv_interval(user, "fetchmail_interval", FETCHMAIL_INTERVAL);

#if defined(FETCHMAIL_WAKEUP_DEBUG)
	i_debug("fetchmail_wakeup: interval %ld used for %s.", fetchmail_interval, ctx->name);
#endif

	/* try rate-limiting only if interval is set to a value > 0 */
	if (fetchmail_interval > 0) {
		if (ratelimit(fetchmail_interval))
			return;

#if defined(FETCHMAIL_WAKEUP_DEBUG)
		i_debug("fetchmail_wakeup: rate limit passed.");
#endif
	}

	/* if a helper application is defined, then call it */
	if ((fetchmail_helper != NULL) && (*fetchmail_helper != '\0')) {
		pid_t pid;
		int status;
		char *const *argv;

		i_info("fetchmail_wakeup: executing helper %s.", fetchmail_helper);

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

		i_info("fetchmail_wakeup: sending SIGUSR1 to process given in %s.", fetchmail_pidfile);

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
 * IMAPv4 command wrapper / pre-command hook callback:
 * - Dovecot 2.1+: simply call fetchmail_wakeup, as Dovecot 2.1+ has command hooks
 */
static void fetchmail_wakeup_cmd(struct client_command_context *ctx)
{
	if (ctx != NULL && ctx->name != NULL && ctx->client != NULL && ctx->client->user != NULL) {
		struct mail_user *user = ctx->client->user;
		const char *fetchmail_cmds = mail_user_plugin_getenv(user, "fetchmail_commands");
		const char **cmds = default_cmds;
		int i;

		/* use configured commands if available */
		if (fetchmail_cmds != NULL)
			cmds = t_strsplit_spaces(fetchmail_cmds, ", ");

		for (i = 0; cmds != NULL && cmds[i] != NULL; i++) {
			if (strcasecmp(cmds[i], ctx->name) == 0) {
				const char *username = (user->username != NULL)
						       ? user->username
						       : "(unknown user)";

				i_info("fetchmail_wakeup: intercepting %s for %s.",
				       cmds[i], username);

				/* try to wake up fetchmail */
				fetchmail_wakeup(ctx);

				break;
			}
		}
	}
}


/*
 * IMAPv4 post-command hook callback:
 * - Dovecot 2.1+ (only): required (the hook handlers don't check for NULL), but not used
 */
static void fetchmail_wakeup_null(struct client_command_context *ctx)
{
        /* unused */
}


/*
 * Plugin init: register callback functions into the into command hook chain
 */
void fetchmail_wakeup_plugin_init(struct module *module)
{
	command_hook_register(fetchmail_wakeup_cmd, fetchmail_wakeup_null);

	i_info("fetchmail_wakeup: start intercepting IMAP commands.");
}

/*
 * Plugin deinit: un-register previously registered callback functions
 */
void fetchmail_wakeup_plugin_deinit(void)
{
	command_hook_unregister(fetchmail_wakeup_cmd, fetchmail_wakeup_null);

	i_info("fetchmail_wakeup: stop intercepting IMAP commands.");
}


/*
 * declare runtime dependency on IMAP
 */
const char fetchmail_wakeup_plugin_binary_dependency[] = "imap";

/* EOF */
