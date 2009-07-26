# Makefile for fetchmail_wakeup

#### configuration begin ####

# directories #
DOVECOT_INC_PATH = /usr/include/dovecot
DOVECOT_IMAP_PLUGIN_PATH = /usr/lib/dovecot/modules/imap
BINDIR = /usr/bin
MAN1DIR = /usr/share/man/man1
MAN7DIR = /usr/share/man/man7

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

build: ${PLUGIN_NAME} ${HELPER_NAME}

${PLUGIN_NAME}: ${PLUGIN_SOURCES}
	$(CC) -fPIC -shared -Wall \
	    -I${DOVECOT_INC_PATH} \
	    -I${DOVECOT_INC_PATH}/src \
	    -I${DOVECOT_INC_PATH}/src/lib \
	    -I${DOVECOT_INC_PATH}/src/lib-storage \
	    -I${DOVECOT_INC_PATH}/src/lib-mail \
	    -I${DOVECOT_INC_PATH}/src/lib-imap \
	    -DHAVE_CONFIG_H \
	    $< -o $@

${HELPER_NAME}: ${HELPER_SOURCES}

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
	$(RM) ${PLUGIN_NAME} ${HELPER_NAME}

# EOF
