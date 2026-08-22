Fetchmail notification IMAP plugin for Dovecot
==============================================

Description
-----------
fetchmail_wakeup is a plugin for Dovecot that tries to wake fetchmail
whenever a specific IMAP command is received by the mail server.

This way, by using the mail client to interact with your local mail server,
you can fetch mails from one or many remote "upstream" mail servers into
your local mail server without having to explicitly trigger fetchmail.

This allows you to stay in the mail client and get new mails from "upstream"
as quickly as possible.

It allows intercepting the IMAP commands

- **STATUS**: client polls for new mail
- **IDLE**:   client tells server to push new mail
- **NOOP**:   client allows server to notify on new mail
- **NOTIFY**: client tells server to inform on specific events

and tries to wake up a running fetchmail daemon before performing
the IMAP operation.

fetchmail_wakeup supports two different ways of waking up a fetchmail
daemon:

- sending the SIGUSR1 signal to a process whose process ID (PID) is read
  from the first line of a so-called PID file
- executing a command

As both, the PID file and the command to execute, can be configured in
Dovecot's configuration file dovecot.conf, you are not restricted to
fetchmail, but can execute or send a signal to any command you want.

Although some kind of runtime-configurable rate-limiting is supported,
it is limited to being per user and session only.

This plugin is explicitly not considered suitable for large scale
installations. But those installations do not rely on fetchmail for pulling
in new mail from remote mail servers anyway.


Copyrights
----------
Copyright (C) 2007 Guillaume Chazarain <guichaz@yahoo.fr>

- original version named wake_up_fetchmail.c

Copyright (C) 2009-2026 Peter Marschall <peter@adpm.de>

- adaptions to Dovecot 1.1, 1.2 [both deprecated], and 2.x
- rename to fetchmail_wakeup.c
- configuration via Dovecot configuration
- flexible, Dovecot 2.4 compliant variable expansion

Copyright (C) 2026 Johan Kunnen <johan@kunnen.frl>

- adaptions to Dovecot 2.4 config
- original %h expansion for fetchmail_wakeup_pidfile


License
-------
LGPL v2.1


Installation
------------
Installation of the plugin is essentially a 3-step process:

1) Configure the paths in the Makefile according to your installation

   The relevant variables are:

   - **DOVECOT_INCDIR** - directory containing Dovecot's header files
   - **DOVECOT_MODULEDIR** - directory the main plugin shall be installed into
   - **DOVECOT_SETTINGSDIR** - directory the settings plugin shall be installed into
   - **DOVECOT_ETCDIR** - directory where dovecot.conf resides
   - **BINDIR** - directory the helper program shall be installed into
   - **MAN1DIR** - directory the helper manunal page shall be installed into
   - **MAN7DIR** - directory the plugin's manual page shall be installed into
   - **FETCHMAIL_PIDFILE** - fully qualified path of fetchmail's PID file
   - **DEBUG** - if you want to see what's going on

2) Compile the module with the following command line

```shell
   make build
```

3) Then install the resulting files to their target directories.

   This can be achieved using:

```shell
   make install
```

That's it.


Configuration
-------------
After the plugin module is installed and ready to be used, Dovecot's
configuration file, usually `/etc/dovecot/dovecot.conf`, needs to be adapted
to make Dovecot use the plugin.

This is a 2-step process:

1) Enable the plugin in Dovecot.

   This is done in dovecot.conf's `protocol imap` section and comprises
   extending the `mail_plugins` section by inserting `fetchmail_wakeup = yes`:

   ```dovecot.conf
   protocol imap {
     mail_plugins {
       fetchmail_wakeup = yes
       #...
     }

     # ...
   }
   ```

2) Set the plugin's configuration options.

   fetchmail_wakeup supports four configuration options:

   - **`fetchmail_wakeup_commands =`** *COMMAND-LIST*

     Comma-separated list of IMAP commands to intercept.
     If not given, the default is the complete list of IAMP commands
     accepted to intercept, i.e. `STATUS`, `IDLE`, `NOOP`, and `NOTIFY`
     as described above.

   - **`fetchmail_wakeup_interval =`** *NUMBER*

     Set minimal interval between two fetchmail invocations to *NUMBER*
     seconds. If it is not given, the interval defaults to `0`, which disables
     rate-limiting.

   - **`fetchmail_wakeup_helper =`** *COMMAND*

     Execute *COMMAND* to either start fetchmail (or any other mail fetching
     tool), or to awaken a running fetchmail daemon (or any other mail
     fetching tool).

   - **`fetchmail_wakeup_pidfile =`** *NAME*

     Use *NAME* as the file to read the process ID (PID) of a running fetchmail
     instance from, and awaken this instance by sending it the SIGUSR1 signal.

   If `fetchmail_wakeup_helper` is given, it takes precedence; in this case
   `fetchmail_wakeup_pidfile` is ignored.

   ```dovecot.conf
   # ...

   ## fetchmail_wakeup plugin: awaken fetchmail on IMAP commands

   # IMAP commands to intercept
   # (this setting is optional and defaults to STATUS, IDLE, NOOP, NOTIFY)
   fetchmail_wakeup_commands = STATUS, IDLE, NOOP, NOTIFY

   # minimal interval (in seconds) between two awakenings of fetchmail
   # (this setting is optional and defaults to 0, disabling rate-limiting)
   fetchmail_wakeup_interval = 60

   # a helper program to notify fetchmail accordingly
   fetchmail_wakeup_helper = /usr/bin/awaken-fetchmail

   # pid file of a running fetchmail instance
   fetchmail_wakeup_pidfile = %{home}/.fetchmail.pid

   # ...
   ```

   The two file-related configuration options `fetchmail_wakeup_helper` and
   `fetchmail_wakeup_pidfile` support variables:

   - `%{user}` - name of the user raising the IMAP command
   - `%{home}` - home directory of the user raising the IMAP command

   Starting with Dovecot 2.0, the config file can be split into small parts that
   are consolidated automatically by Dovecot.
   To support this mode of splitting config files, an example config snippet is
   provided in

```
     example-config/conf.d/90-fetchmail_wakeup.conf
```

Now restart your Dovecot daemon and enjoy the comfort of being able to control
fetchmail by simply fetching mail from the server in your IMAP client.


Versions
--------
- Versions 2.4.x are targeted towards Dovevot 2.4.x
  and give up compatibility with Dovecot < 2.4
- Versions 2.2.x are targetted towards Dovecot 2.2.x
  and give up compatibility with Dovecot < 2.1
- Versions 2.x are targetted towards Dovecot 2.x.
- Versions 1.x are intended to be used with Dovecot 1.x


Tests
-----

- Version 2.4.0 tested with Dovecot 2.4.1 and 2.4.4
- Version 2.3.1 tested with Dovecot 2.3.18
- Previous versions tested with Dovecot 1.1.13, 1.1.16, various 1.2.x, 2.0.x, 2.1.x, 2.2.9 versions
- Original version tested with Dovecot-1.0.3


GIT archive structure
---------------------
dovecot-fetchmail's git archive consists of 3 branches:

- original: contains the original import of Guillaume's file wake_up_fetchmail.c
- master: contains the main upstream development branch
- debian: contains the extensions to create a Debian package using git-buildpackage

