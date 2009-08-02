# Makefile for fetchmail_wakeup

#### configuration begin ####

# Dovecot's header directory
DOVECOT_INC_PATH = /usr/include/dovecot
# Dovecot's IMAP plugin path
DOVECOT_IMAP_PLUGIN_PATH = /usr/lib/dovecot/modules/imap
# Dovecot's config directory (where dovecot.conf resides)
DOVECOT_ETCDIR = /etc/dovecot
# directory for binaries
BINDIR = /usr/bin
# directories for man pages sections 1 & 7
MAN1DIR = /usr/share/man/man1
MAN7DIR = /usr/share/man/man7
# fetchmail's PID file (used in awaken-fetchmail)
FETCHMAIL_PIDFILE = /var/run/fetchmail/fetchmail.pid

## usually no need to configure anything below this line ##

# plugin source & target name #
PLUGIN_SOURCES = fetchmail_wakeup.c
PLUGIN_NAME = lib_fetchmail_wakeup_plugin.so

# helper sources, target name & setuid account #
HELPER_SOURCES = awaken-fetchmail.c
HELPER_NAME = awaken-fetchmail
HELPER_USER = fetchmail

# manual pages in their respective sections #
MAN1PAGES = awaken-fetchmail.1
MAN7PAGES = fetchmail_wakeup.7

#### configuration end ####


.PHONY: all build install install_man clean

all: build

build: ${PLUGIN_NAME} ${HELPER_NAME} ${MAN1PAGES} ${MAN7PAGES}

${PLUGIN_NAME}: ${PLUGIN_SOURCES}
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) \
	      -fPIC -shared -Wall \
	      -I${DOVECOT_INC_PATH} \
	      -I${DOVECOT_INC_PATH}/src \
	      -I${DOVECOT_INC_PATH}/src/lib \
	      -I${DOVECOT_INC_PATH}/src/lib-storage \
	      -I${DOVECOT_INC_PATH}/src/lib-mail \
	      -I${DOVECOT_INC_PATH}/src/lib-imap \
	      -DHAVE_CONFIG_H \
	      $< -o $@

${HELPER_NAME}: ${HELPER_SOURCES}
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) \
	      -Wall \
	      -D'FETCHMAIL_PIDFILE="${FETCHMAIL_PIDFILE}"' \
	      $< -o $@

%.1 : %.1.in
	sed -e 's:DOVECOT_IMAP_PLUGIN_PATH:${DOVECOT_IMAP_PLUGIN_PATH}:g' \
	    -e  's:BINDIR:${BINDIR}:g' \
	    -e  's:MAN1DIR:${MAN1DIR}:g' \
	    -e  's:MAN7DIR:${MAN7DIR}:g' \
	    -e  's:DOVECOT_ETCDIR:${DOVECOT_ETCDIR}:g' \
	    -e  's:FETCHMAIL_PIDFILE:${FETCHMAIL_PIDFILE}:g' \
	    -e  's:PLUGIN_NAME:${PLUGIN_NAME}:g' \
	$< > $@

%.7 : %.7.in
	sed -e 's:DOVECOT_IMAP_PLUGIN_PATH:${DOVECOT_IMAP_PLUGIN_PATH}:g' \
	    -e  's:BINDIR:${BINDIR}:g' \
	    -e  's:MAN1DIR:${MAN1DIR}:g' \
	    -e  's:MAN7DIR:${MAN7DIR}:g' \
	    -e  's:DOVECOT_ETCDIR:${DOVECOT_ETCDIR}:g' \
	    -e  's:FETCHMAIL_PIDFILE:${FETCHMAIL_PIDFILE}:g' \
	    -e  's:PLUGIN_NAME:${PLUGIN_NAME}:g' \
	$< > $@

install: install_plugin install_helper install_man

install_plugin: ${PLUGIN_NAME}
	install -d ${DESTDIR}/${DOVECOT_IMAP_PLUGIN_PATH}
	install $< ${DESTDIR}/${DOVECOT_IMAP_PLUGIN_PATH}

install_helper: ${HELPER_NAME}
	install -d ${DESTDIR}/${BINDIR}
	install -o ${HELPER_USER} -m 4755 $< ${DESTDIR}/${BINDIR}

install_man: install_man1 install_man7

install_man1: ${MAN1PAGES}
	install -d ${DESTDIR}/${MAN1DIR}
	install $? ${DESTDIR}/${MAN1DIR}

install_man7: ${MAN7PAGES}
	install -d ${DESTDIR}/${MAN7DIR}
	install $? ${DESTDIR}/${MAN7DIR}

clean:
	$(RM) ${PLUGIN_NAME} ${HELPER_NAME} ${MAN1PAGES} ${MAN7PAGES}

# EOF
