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

#ifndef FETCHMAIL_WAKEUP_SETTINGS_PLUGIN_H
#define FETCHMAIL_WAKEUP_SETTINGS_PLUGIN_H

#if !defined(FETCHMAIL_INTERVAL)
#  define FETCHMAIL_INTERVAL	0
#endif

#if !defined(FETCHMAIL_PIDFILE)
#  define FETCHMAIL_PIDFILE	"/run/fetchmail/fetchmail.pid"
#endif


enum fetchmail_command {
	FETCHMAIL_NO_COMMAND		= 0x00,
	FETCHMAIL_COMMAND_NOOP		= 0x01,
	FETCHMAIL_COMMAND_STATUS	= 0x02,
	FETCHMAIL_COMMAND_IDLE		= 0x04,
	FETCHMAIL_COMMAND_NOTIFY	= 0x08,
	FETCHMAIL_ALL_COMMANDS		= ( FETCHMAIL_COMMAND_NOOP | FETCHMAIL_COMMAND_STATUS | FETCHMAIL_COMMAND_IDLE | FETCHMAIL_COMMAND_NOTIFY )
};

static const char *fetchmail_command_names[] = {
	"noop",
	"status",
	"idle",
	"notify",
	NULL
};

struct fetchmail_wakeup_settings {
	pool_t pool;

	ARRAY_TYPE(const_string) fetchmail_commands;
	unsigned int fetchmail_interval;
	const char *fetchmail_helper;
	const char *fetchmail_pidfile;

	enum fetchmail_command parsed_commands;
};

const struct setting_parser_info *get_setting_parser_info(void);

#endif
