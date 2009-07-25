# Makefile for fetchmail_wakeup

SOURCES = fetchmail_wakeup.c
DOVECOT_INC_PATH = /usr/include/dovecot
DOVECOT_IMAP_PLUGIN_PATH = /usr/lib/dovecot/modules/imap
PLUGIN_NAME = lib_fetchmail_wakeup_plugin.so

.PHONY: all build install clean

all: build

build: ${PLUGIN_NAME}

${PLUGIN_NAME}: ${SOURCES}
	$(CC) -fPIC -shared -Wall \
	    -I${DOVECOT_INC_PATH} \
	    -I${DOVECOT_INC_PATH}/src \
	    -I${DOVECOT_INC_PATH}/src/lib \
	    -I${DOVECOT_INC_PATH}/src/lib-storage \
	    -I${DOVECOT_INC_PATH}/src/lib-mail \
	    -I${DOVECOT_INC_PATH}/src/lib-imap \
	    -DHAVE_CONFIG_H \
	    $< -o $@

install: ${PLUGIN_NAME}
	install $< ${DESTDIR}/${DOVECOT_IMAP_PLUGIN_PATH}

clean:
	$(RM) lib_fetchmail_wakeup_plugin.so

# EOF
