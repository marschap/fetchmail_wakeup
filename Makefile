# Makefile for fetchmail_wakeup

DOVECOT_INC_PATH = /usr/include/dovecot
DOVECOT_IMAP_PLUGIN_PATH = /usr/lib/dovecot/modules/imap
BINDIR = /usr/bin

PLUGIN_SOURCES = fetchmail_wakeup.c
PLUGIN_NAME = lib_fetchmail_wakeup_plugin.so

HELPER_SOURCES = awaken-fetchmail.c
HELPER_NAME = awaken-fetchmail

.PHONY: all build install clean

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

install: install_plugin install_helper

install_plugin: ${PLUGIN_NAME}
	install -d ${DESTDIR}/${DOVECOT_IMAP_PLUGIN_PATH}
	install $< ${DESTDIR}/${DOVECOT_IMAP_PLUGIN_PATH}

install_helper: ${HELPER_NAME}
	install -d ${DESTDIR}/${BINDIR}
	install -m 4755 $< ${DESTDIR}/${BINDIR}

clean:
	$(RM) ${PLUGIN_NAME} ${HELPER_NAME}

# EOF
