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

#include <stdio.h>
#include <stdlib.h>
#include "lib.h"
#include "array.h"
#include "settings.h"
#include "module-dir.h"
#include "settings-parser.h"
#include "fetchmail_wakeup_settings.h"

const char *fetchmail_wakeup_settings_plugin_version = DOVECOT_ABI_VERSION;
extern const struct setting_parser_info fetchmail_wakeup_setting_parser_info;
extern const struct setting_parser_info *fetchmail_wakeup_plugin_setting_infos[];

#undef DEF
#define DEF(type, name) \
	SETTING_DEFINE_STRUCT_##type(#name, name, struct fetchmail_wakeup_settings)
static const struct setting_define fetchmail_wakeup_setting_defines[] = {
	DEF(BOOLLIST, fetchmail_commands),
	DEF(UINT, fetchmail_interval),
	DEF(STR, fetchmail_helper),
	DEF(STR, fetchmail_pidfile),

	SETTING_DEFINE_LIST_END
};

static const struct fetchmail_wakeup_settings fetchmail_wakeup_default_settings = {
	.fetchmail_commands =  ARRAY_INIT,
	.fetchmail_interval = 60,
	.fetchmail_helper = "",
	.fetchmail_pidfile = "/run/fetchmail/fetchmail.pid",
};

static const struct setting_keyvalue fetchmail_wakeup_default_settings_keyvalue[] = {
	{ "fetchmail_commands/status", "yes" },
	{ "fetchmail_commands/idle", "yes" },
	{ "fetchmail_commands/noop", "yes" },
	{ "fetchmail_commands/notify", "yes" },
	{ NULL, NULL }
};

static bool fetchmail_wakeup_settings_check(void *_set, pool_t pool, const char **error_r);

static enum fetchmail_command fetchmail_command_find(const char *name)
{
	unsigned int i;

	for (i = 0; fetchmail_command_names[i] != NULL; i++) {
		if (strcmp(name, fetchmail_command_names[i]) == 0) {
			return 1 << i;
		}
	}
	return 0;
}

static int wakeup_parse_commands(const ARRAY_TYPE(const_string) *arr,
				 enum fetchmail_command *commands_r, const char **error_r)
{
	const char *str;
	enum fetchmail_command command;

	*commands_r = 0;
	array_foreach_elem(arr, str) {
		command = fetchmail_command_find(str);
		if (command == 0) {
			*error_r = t_strdup_printf(
				"Unknown command in fetchmail_commands: '%s'", str);
			return -1;
		}
		*commands_r |= command;
	}
	if (*commands_r == 0) {
		*commands_r = FETCHMAIL_COMMAND_NOOP | FETCHMAIL_COMMAND_STATUS | FETCHMAIL_COMMAND_IDLE | FETCHMAIL_COMMAND_NOTIFY;
	}
	return 0;
}

const struct setting_parser_info fetchmail_wakeup_setting_parser_info = {
	.name = "fetchmail_wakeup",
	.plugin_dependency = "lib_fetchmail_wakeup_settings_plugin",

	.defines = fetchmail_wakeup_setting_defines,
	.defaults = &fetchmail_wakeup_default_settings,
	.default_settings = fetchmail_wakeup_default_settings_keyvalue,
	.check_func = fetchmail_wakeup_settings_check,

	.struct_size = sizeof(struct fetchmail_wakeup_settings),
	.pool_offset1 = 1 + offsetof(struct fetchmail_wakeup_settings, pool),
};

static bool fetchmail_wakeup_settings_check(void *_set, pool_t pool ATTR_UNUSED, const char **error_r) 
{
	struct fetchmail_wakeup_settings *set = _set;
	
	if (wakeup_parse_commands(&set->fetchmail_commands, &set->parsed_commands, error_r)) {
		return FALSE;
	}
	return TRUE;
}

/*
 * Plugin init
 */
void fetchmail_wakeup_settings_plugin_init(struct module *module ATTR_UNUSED)
{
	/* nope */
}

/*
 * Plugin deinit
 */
void fetchmail_wakeup_settings_plugin_deinit(void)
{
	/* nope */
}

const struct setting_parser_info fetchmail_wakeup_setting_parser_info;

const struct setting_parser_info *fetchmail_wakeup_settings_plugin_set_infos[] = {
	&fetchmail_wakeup_setting_parser_info,
	NULL
};

const struct setting_parser_info *get_setting_parser_info(void) {
	return &fetchmail_wakeup_setting_parser_info;
}

/* EOF */
