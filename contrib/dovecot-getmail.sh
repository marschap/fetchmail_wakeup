#!/bin/bash
# Script dovecot-getmail.sh: When called by the dovecot fetchmail-wakeup
# plugin, this script launches a getmail mail check in the background.

# (C) August Mayer, 2011. This script is public domain, under the
# Creative Commons CC0 license, http://creativecommons.org/publicdomain/zero/1.0/

# Needs an entry in /etc/sudoers, for starting as the target user:
#
#   dovecot ALL=(jack) NOPASSWD: /home/jack/bin/dovecot-getmail.sh run
#
# Reason: Dovecot runs as user dovecot (at least under Debian), but getmail
# should run as the user (named "jack" here, as an example) whose
# mail is being checked.

# Needs the /usr/bin/lockfile program, included with procmail.


# Name of the user whose mail to check.
USER=jack

# Arguments for getmail.
GETMAIL_ARGS="-v -r mailaccount1.rc -r mailaccount2.rc"

# Wait time after getting mail, to prevent getting mail too often.
WAITTIME=30

# Logfile where to write log messages.
LOG=/var/log/dovecot-getmail.log

# Lock file that will be created during getmail runs.
LOCKFILE=/tmp/get-all-mail.lockfile

# Getmail program.
GETMAIL=/usr/bin/getmail


# Restart in the background, so that it does not block dovecot.
if [ "$1" != "run" ]; then
	nohup sudo -u $USER $0 run >>$LOG 2>&1 &
	exit
fi

{
	# Create lock file.
	if lockfile -r 0 $LOCKFILE; then
		echo "Acquired lock."
	else
		echo "Already running, exiting second instance."
		exit
	fi

	# Call getmail.
	echo $(date) "Checking mail with getmail..."
	$GETMAIL $GETMAIL_ARGS
	echo "done."

	# Wait.
	echo "Waiting a bit..."
	sleep $WAITTIME

	# Remove lockfile.
	echo "Removing lockfile."
	rm -f $LOCKFILE

} >>$LOG 2>&1


