/*
 * Fetchmail notification IMAP plugin for Dovecot
 *
 * Copyright (C) 2007 Guillaume Chazarain <guichaz@yahoo.fr>
 * - original version named wake_up_fetchmail.c
 *
 * Copyright (C) 2009-2026 Peter Marschall <peter@adpm.de>
 * - adaptions to dovecot 1.1, 1.2 [both deprecated], and 2.x
 * - rename to fetchmail_wakeup.c
 * - configuration via dovecot.config
 *
 * Copyright (C) 2026 Johan Kunnen <johan@kunnen.frl>
 * - adaptions to dovecot 2.4 config
 * - %h expansion in fetchmail_wakeup_pidfile
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
#include "array.h"
#include "imap-client.h"
#include "ioloop.h"
#include "settings.h"
#include "fetchmail_wakeup_settings.h"


/* check that we have the minimal dovecot version required for compilation */
#if ! DOVECOT_PREREQ(2, 4, 0)
#      error *** dovecot version too low: must be 2.4.0 or higher ***
#endif


/*
 * make sure we have the right ABI version at runtime
 */
const char *fetchmail_wakeup_plugin_version = DOVECOT_ABI_VERSION;


/*
 * Don't bother waking up fetchmail too often
 */
static bool ratelimit(long interval)
{
	static time_t previous;

	if (ioloop_time - previous > interval) {
		previous = ioloop_time;
		return FALSE;
	}

	return TRUE;
}

static int replace_percent_home(const char* string_possibly_containing_home, struct mail_user* user, const char** result_r)
{
	char buf[1024] = "";;
	char current;
	char prev;
	int k = 0;

	for (int i = 0; string_possibly_containing_home[i] != '\0'; i++) {
		current = string_possibly_containing_home[i];
		if (current == 'h' && prev == '%') {
			const char *home_dir;
			if (mail_user_get_home(user, &home_dir) <= 0) {
				return -1;
			}
			k--; // overwrite already written %
			for (int j = 0; home_dir[j] != '\0'; j++) {
				buf[k] = home_dir[j];
				k++;
			}
		}
		else {
			buf[k] = current;
			k++;
		}
		prev = current;
	}
	buf[k] = '\0';
	*result_r = t_strdup_printf("%s", buf);
	return 1;
}


/*
 * Send a signal to fetchmail or call a helper to awaken fetchmail
 */
static void fetchmail_wakeup(struct client_command_context *ctx)
{
	struct mail_user *user = ctx->client->user;	/* != NULL as checked by caller */
	const char *fetchmail_wakeup_helper = NULL;
	const char *fetchmail_wakeup_pidfile = NULL;
	long fetchmail_wakeup_interval = FETCHMAIL_INTERVAL;
	const struct fetchmail_wakeup_settings *set;
	const char *error;

	if (settings_get(user->event, get_setting_parser_info(), 0, &set, &error) < 0) {
		e_error(user->event, "%s", error);
		return;
	}
	fetchmail_wakeup_interval = set->fetchmail_wakeup_interval;

#if defined(FETCHMAIL_WAKEUP_DEBUG)
	i_debug("fetchmail_wakeup: interval %ld used for %s.", fetchmail_wakeup_interval, ctx->name);
#endif

	/* try rate-limiting only if interval is set to a value > 0 */
	if (fetchmail_wakeup_interval > 0) {
		if (ratelimit(fetchmail_wakeup_interval))
			return;

#if defined(FETCHMAIL_WAKEUP_DEBUG)
		i_debug("fetchmail_wakeup: rate limit passed.");
#endif
	}

	fetchmail_wakeup_helper = set->fetchmail_wakeup_helper;
	fetchmail_wakeup_pidfile = set->fetchmail_wakeup_pidfile;

	settings_free(set);

	/* if a helper application is defined, then call it */
	if ((fetchmail_wakeup_helper != NULL) && (*fetchmail_wakeup_helper != '\0')) {
		pid_t pid;
		int status;
		char *const *argv;

		i_info("fetchmail_wakeup: executing helper %s.", fetchmail_wakeup_helper);

		switch (pid = fork()) {
			case -1:	// fork failed
				i_warning("fetchmail_wakeup: fork() failed");
				return;
			case 0:		// child
				argv = (char *const *) t_strsplit_spaces(fetchmail_wakeup_helper, " ");
				if ((argv != NULL) && (*argv != NULL)) {
					execv(argv[0], argv);
					i_warning("fetchmail_wakeup: execv(%s) failed: %s",
						argv[0], strerror(errno));
					exit(1);
				}
				else {
					i_warning("fetchmail_wakeup: illegal fetchmail_wakeup_helper");
					exit(1);
				}
			default:	// parent
				waitpid(pid, &status, 0);
		}
	}
	/* otherwise if a pid file name is given, signal fetchmail with that pid */
	else if ((fetchmail_wakeup_pidfile != NULL) && (*fetchmail_wakeup_pidfile != '\0')) {
		const char *fetchmail_pid;
		FILE *pidfile = NULL;

		if (replace_percent_home(fetchmail_wakeup_pidfile, user, &fetchmail_pid) < 0) {
			i_warning("fetchmail_wakeup: error finding home for user %s.", user->username);
			return;
		}
		pidfile = fopen(fetchmail_pid, "r");

		i_info("fetchmail_wakeup: sending SIGUSR1 to process given in %s.", fetchmail_pid);

		if (pidfile) {
			pid_t pid = 0;

			if ((fscanf(pidfile, "%d", &pid) == 1) && (pid > 1))
				kill(pid, SIGUSR1);
			else
				i_warning("fetchmail_wakeup: error reading valid pid from %s",
					fetchmail_pid);
			fclose(pidfile);
		}
		else {
			i_warning("fetchmail_wakeup: error opening %s",
				 fetchmail_wakeup_pidfile);
		}
	}
	/* otherwise warn on missing configuration */
	else {
		i_warning("fetchmail_wakeup: neither fetchmail_wakeup_pidfile nor fetchmail_wakeup_helper given");
	}
}


/*
 * IMAP command wrapper / pre-command hook callback:
 * - Dovecot 2.1+: simply call fetchmail_wakeup, as Dovecot 2.1+ has command hooks
 */
static void fetchmail_wakeup_cmd(struct client_command_context *ctx)
{
	if (ctx != NULL && ctx->name != NULL && ctx->client != NULL && ctx->client->user != NULL) {
		struct mail_user *user = ctx->client->user;
		const struct fetchmail_wakeup_settings *set;
		const char *error;
		enum fetchmail_command fetchmail_cmds = FETCHMAIL_NO_COMMAND;
		enum fetchmail_command cmd = 1;

		if (settings_get(user->event, get_setting_parser_info(), 0, &set, &error) < 0) {
			e_error(user->event, "%s", error);
			return;
		}
		fetchmail_cmds = set->parsed_commands;

		settings_free(set);

		for (unsigned int i = 0; fetchmail_command_names[i] != NULL; i++) {
			if ((fetchmail_cmds & cmd) && (strcasecmp(fetchmail_command_names[i], ctx->name) == 0)) {
				const char *username = (user->username != NULL) ? user->username : "(unknown user)";

				i_info("fetchmail_wakeup: intercepting %s for %s.", fetchmail_command_names[i], username);

				/* try to wake up fetchmail */
				fetchmail_wakeup(ctx);

				break;
			}
			/* left-shift cmd */
			cmd <<= 1;
		}
	}
}


/*
 * IMAP post-command hook callback:
 * - Dovecot 2.1+ (only): required (the hook handlers don't check for NULL), but not used
 */
static void fetchmail_wakeup_null(struct client_command_context *ctx)
{
	/* unused */
}


/*
 * Plugin init: register callback functions into the into command hook chain
 */
void fetchmail_wakeup_plugin_init(struct module *module ATTR_UNUSED)
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
