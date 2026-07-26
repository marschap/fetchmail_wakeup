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

#include <sys/types.h>
#include <ctype.h>
#include "lib.h"
#include "module-dir.h"
#include "settings.h"
#include "settings-parser.h"

enum fetchmail_command {
	FETCHMAIL_COMMAND_NOOP   	= 0x01,
	FETCHMAIL_COMMAND_STATUS 	= 0x02,
	FETCHMAIL_COMMAND_IDLE   	= 0x04,
	FETCHMAIL_COMMAND_NOTIFY 	= 0x08
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


/*
 * Plugin init
 */
void fetchmail_wakeup_settings_plugin_init(struct module *module ATTR_UNUSED);

/*
 * Plugin deinit
 */
void fetchmail_wakeup_settings_plugin_deinit(void);

#endif

/* EOF */
