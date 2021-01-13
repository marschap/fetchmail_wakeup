/*
 * Fetchmail notification IMAP plugin for Dovecot
 *
 * Copyright (C) 2007 Guillaume Chazarain <guichaz@yahoo.fr>
 * - original version named wake_up_fetchmail.c
 *
 * Copyright (C) 2009-2013 Peter Marschall <peter@adpm.de>
 * - adaptions to dovecot 1.1, 1.2 [now deprecated], 2.0, 2.1 & 2.2
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


/* make sure only one API version is defined (prefer higher ones) */
#if defined(DOVECOT_PLUGIN_API_2_1)
#  undef   DOVECOT_PLUGIN_API_2_0
typedef    void   handler_t;
#elif defined(DOVECOT_PLUGIN_API_2_0)
#  warning "======== Using Dovecot 2.0 plugin API ========"
typedef    bool  handler_t;
#else
#  define  DOVECOT_PLUGIN_API_2_1
#  warning "======== Defaulting to Dovecot 2.1+ plugin API ========"
typedef    void   handler_t;
#endif


#define FETCHMAIL_INTERVAL	60


/* data structure for commands to be overridden */
struct overrides {
  const char *name;		/* the IMAPv4 command name */
  struct command orig_cmd;	/* copy of the original command's data structure */
  struct timeval last;          /* last time this command was executed */
};


/* commands that can be overridden */
static struct overrides cmds[] = {
  { "IDLE",   {}, {} },
  { "NOOP",   {}, {} },
  { "STATUS", {}, {} },
  { "NOTIFY", {}, {} },
  { NULL,     {}, {} }
};


/* get the index of the given command in cmds[] */

static int get_cmd_index(const char* name) {
  int i=0;
  for (i = 0; cmds[i].name != NULL; i++) {
    if (strcasecmp(cmds[i].name,name) == 0) {
      return i;
    }
  }
  return i;
}

/*
 * Get a interval value from config and parse it into a number (with fallback for failures)
 */
static long getenv_interval(struct mail_user *user, const char *name, long fallback)
{
	if (name != NULL) {
		const char *value_as_str = mail_user_plugin_getenv(user, name);

		if (value_as_str != NULL) {
			long value;

			if ((str_to_long(value_as_str, &value) < 0) || (value <= 0)) {
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
static bool ratelimit(long interval, const char* name)
{
	struct timeval last_one;
	struct timeval now;
        int cmd_index;
	long long millisec_delta;

	if (gettimeofday(&now, NULL))
		return FALSE;

        cmd_index = get_cmd_index(name);
        last_one = cmds[cmd_index].last;
	millisec_delta = ((now.tv_sec - last_one.tv_sec) * 1000000LL +
	                  now.tv_usec - last_one.tv_usec) / 1000LL;
	if (millisec_delta > interval * 1000LL) {
		cmds[cmd_index].last = now;
		return FALSE;
	}
	return TRUE;
}


/*
 * Send a signal to fetchmail or call a helper to awaken fetchmail
 */
static void fetchmail_wakeup(struct client_command_context *ctx)
{
	struct client *client = ctx->client;
	long interval, fetchmail_interval = FETCHMAIL_INTERVAL;
        char cfg_var[28];

	/* make sure client->user is defined */
	if (client == NULL || client->user == NULL)
		return;

	/* read config variables depending on the session */
	const char *fetchmail_helper = mail_user_plugin_getenv(client->user, "fetchmail_helper");
	const char *fetchmail_pidfile = mail_user_plugin_getenv(client->user, "fetchmail_pidfile");

	/* convert config variable "fetchmail_interval" into a number */
	fetchmail_interval = getenv_interval(client->user, "fetchmail_interval", FETCHMAIL_INTERVAL);

        /* query per command interval setting */
        strcpy(cfg_var,"fetchmail_interval_");
        strncat(cfg_var,ctx->name,6);
        interval = getenv_interval(client->user,cfg_var,fetchmail_interval);
        
#if defined(FETCHMAIL_WAKEUP_DEBUG)
	i_debug("fetchmail_wakeup: interval %ld used for %s.",interval, ctx->name);
#endif
        /* limit rate */
        if (ratelimit(interval,ctx->name))
          return;

#if defined(FETCHMAIL_WAKEUP_DEBUG)
	i_debug("fetchmail_wakeup: rate limit passed.");
#endif

	/* if a helper application is defined, then call it */
	if ((fetchmail_helper != NULL) && (*fetchmail_helper != '\0')) {
		pid_t pid;
		int status;
		char *const *argv;

#if defined(FETCHMAIL_WAKEUP_DEBUG)
		i_debug("fetchmail wakeup: executing %s.", fetchmail_helper);
#endif

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

#if defined(FETCHMAIL_WAKEUP_DEBUG)
		i_debug("fetchmail wakeup: sending SIGUSR1 to process given in %s.", fetchmail_pidfile);
#endif

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
 * - Dovecot 2.0: call fetchmail_wakeup & daisy-chain to the IMAP function call
 * - Dovecot 2.1+: simply call fetchmail_wakeup, as Dovecot 2.1+ has command hooks
 */
static handler_t fetchmail_wakeup_cmd(struct client_command_context *ctx)
{
	if (ctx != NULL && ctx->name != NULL) {
		int i;

		for (i = 0; cmds[i].name != NULL; i++) {
			if (strcasecmp(cmds[i].name, ctx->name) == 0) {

#if defined(FETCHMAIL_WAKEUP_DEBUG)
				i_debug("fetchmail wakeup: intercepting %s.", cmds[i].name);
#endif

				/* try to wake up fetchmail */
				fetchmail_wakeup(ctx);

#if defined(DOVECOT_PLUGIN_API_2_0)
				/* daisy chaining: call original IMAPv4 command handler */
				return ((cmds[i].orig_cmd.func != NULL)
					? cmds[i].orig_cmd.func(ctx)
					: FALSE);
#else
				break;
#endif
			}
		}
	}
#if defined(DOVECOT_PLUGIN_API_2_0)
	return FALSE;
#endif
}


/*
 * IMAPv4 post-command hook callback:
 * - Dovecot 2.1+ (only): required (the hook handlers don't check for NULL), but not used
 */
static handler_t fetchmail_wakeup_null(struct client_command_context *ctx)
{
        /* unused */
}


/*
 * Plugin init:
 * - Dovecot 2.0: store original IMAPv4 handler functions and replace it with my own
 * - Dovecot 2.1+: register callback functions into the into command hook chain
 */
void fetchmail_wakeup_plugin_init(struct module *module)
{
#if defined(DOVECOT_PLUGIN_API_2_1)
	command_hook_register(fetchmail_wakeup_cmd, fetchmail_wakeup_null);
#elif defined(DOVECOT_PLUGIN_API_2_0)
	int i;

	/* replace IMAPv4 command handlers by our own */
	for (i = 0; cmds[i].name != NULL; i++) {
		struct command *orig_cmd_ptr = command_find(cmds[i].name);

		if (orig_cmd_ptr != NULL) {
			memcpy(&cmds[i].orig_cmd, orig_cmd_ptr, sizeof(struct command));

			command_unregister(cmds[i].name);
			command_register(cmds[i].name, fetchmail_wakeup_cmd, cmds[i].orig_cmd.flags);
		}
	}
#endif

#if defined(FETCHMAIL_WAKEUP_DEBUG)
	i_debug("fetchmail wakeup: start intercepting IMAP commands.");
#endif
}

/*
 * Plugin deinit:
 * - Dovecot 2.0: restore dovecot's original IMAPv4 handler functions
 * - Dovecot 2.1+: un-register previously registered callback functions
 */
void fetchmail_wakeup_plugin_deinit(void)
{
#if defined(DOVECOT_PLUGIN_API_2_1)
	command_hook_unregister(fetchmail_wakeup_cmd, fetchmail_wakeup_null);
#elif defined(DOVECOT_PLUGIN_API_2_0)
	int i;

	/* restore previous IMAPv4 command handlers */
	for (i = 0; cmds[i].name != NULL; i++) {
		command_unregister(cmds[i].name);
		command_register(cmds[i].orig_cmd.name, cmds[i].orig_cmd.func, cmds[i].orig_cmd.flags);
	}
#endif

#if defined(FETCHMAIL_WAKEUP_DEBUG)
	i_debug("fetchmail wakeup: stop intercepting IMAP commands.");
#endif
}


/*
 * declare dependency on IMAP
 */
const char fetchmail_wakeup_plugin_binary_dependency[] = "imap";

/* EOF */
