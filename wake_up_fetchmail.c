/*
 * Fetchmail notification IMAP plugin for Dovecot
 * Copyright (C) 2007 Guillaume Chazarain <guichaz@yahoo.fr>
 * LGPLv2.1
 *
 * To compile:
cc -fPIC -shared -Wall \
   -I$DOVECOT_PATH \
   -I$DOVECOT_PATH/src \
   -I$DOVECOT_PATH/src/lib-storage \
   -I$DOVECOT_PATH/src/lib-mail \
   -I$DOVECOT_PATH/src/lib-imap \
   -DHAVE_CONFIG_H wake_up_fetchmail.c -o lib_wake_up_fetchmail.so
 *
 * Tested with dovecot-1.0.3
 *
 * To use:
 *   Add to dovecot.conf in section "protocol imap" the line:
 *        mail_plugin_dir = /path/to/directory
 *   where the directory contains lib_wake_up_fetchmail.so. Add also in this
 *   same section the line:
 *        mail_plugins = wake_up_fetchmail
 */

#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "lib/lib.h"
#include "lib/imem.h"
#include "imap/commands.h"

/*
 * Dovecot's real "IDLE" function
 */
static command_func_t *orig_cmd_idle;

/*
 * Dovecot's real "STATUS" function
 */
static command_func_t *orig_cmd_status;

/*
 * "$HOME/.fetchmail.pid"
 */
static char fetchmail_pid_path[PATH_MAX];

/*
 * Don't bother waking up fetchmail twice during 2 seconds
 */
static bool ratelimit(void)
{
	static struct timeval last_one;
	struct timeval now;
	long usecs;

	if (gettimeofday(&now, NULL))
		return FALSE;

	usecs = (now.tv_sec - last_one.tv_sec) * 1000000 +
	    now.tv_usec - last_one.tv_usec;
	if (now.tv_sec - last_one.tv_sec > 2 || usecs >= 2000000) {
		last_one = now;
		return FALSE;
	}

	return TRUE;
}

/*
 * Send a signal to fetchmail
 */
static void wake_up_fetchmail(struct client_command_context *cmd)
{
	FILE *fetchmail_pid_file;
	pid_t fetchmail_pid = 0;

	if (ratelimit())
		return;

	fetchmail_pid_file = fopen(fetchmail_pid_path, "r");
	if (!fetchmail_pid_file)
		return;

	if (fscanf(fetchmail_pid_file, "%d", &fetchmail_pid) == 1)
		if (fetchmail_pid > 1)
			kill(fetchmail_pid, SIGUSR1);
	fclose(fetchmail_pid_file);
}

/*
 * Our "IDLE" wrapper
 */
static bool new_cmd_idle(struct client_command_context *cmd)
{
	wake_up_fetchmail(cmd);
	return orig_cmd_idle(cmd);
}

/*
 * Our "STATUS" wrapper
 */
static bool new_cmd_status(struct client_command_context *cmd)
{
	wake_up_fetchmail(cmd);
	return orig_cmd_status(cmd);
}

/*
 * Plugin init: remember dovecot's "IDLE" and "STATUS" functions and add our own
 * in place
 */
void wake_up_fetchmail_init(void)
{
	char *home = getenv("HOME");
	int res;

	if (!home)
		return;

	res = snprintf(fetchmail_pid_path, sizeof(fetchmail_pid_path),
		       "%s/.fetchmail.pid", home);
	if (res < 0 || res >= sizeof(fetchmail_pid_path))
		return;

	orig_cmd_idle = command_find("IDLE");
	command_unregister("IDLE");
	command_register("IDLE", new_cmd_idle);

	orig_cmd_status = command_find("STATUS");
	command_unregister("STATUS");
	command_register("STATUS", new_cmd_status);
}

/*
 * Plugin deinit: restore dovecot's "IDLE" and "STATUS" functions
 * The command name is dupped, for its memory location to be accessible even
 * when the plugin is unloaded
 */
void wake_up_fetchmail_deinit(void)
{
	if (orig_cmd_idle) {
		command_unregister("IDLE");
		command_register(i_strdup("IDLE"), orig_cmd_idle);
	}

	if (orig_cmd_status) {
		command_unregister("STATUS");
		command_register(i_strdup("STATUS"), orig_cmd_status);
	}
}
