################################################################################
# Dahsee makefile
# 2012-08-13
################################################################################

include ../config.mk

## Appends user flags.
CFLAGS_ALL += ${CFLAGS}
LDFLAGS_ALL += ${LDFLAGS}

## Application information.
CFLAGS_ALL += -DAPPNAME=\"${APPNAME}\"
CFLAGS_ALL += -DVERSION=\"${VERSION}\"
CFLAGS_ALL += -DAUTHOR=\"${AUTHOR}\"
CFLAGS_ALL += -DYEAR=\"${YEAR}\"
CFLAGS_ALL += -DMANPAGE=\"${MANPAGE}\(${MANSECTION}\)\"

## TODO: must make json.c following C99 instead of C11?
# CFLAGS_ALL += -Wall -Wextra -Wshadow -Winline
CFLAGS_ALL += -Wall -Wextra -Wshadow -Winline -pedantic -std=c11

## For use of getopt() with -pedantic.
CFLAGS_ALL += -D_XOPEN_SOURCE=700
# CFLAGS_ALL += -D_ISOC11_SOURCE

## D-Bus
CFLAGS_ALL += $$(pkg-config --cflags dbus-1)
LDFLAGS_ALL += $$(pkg-config --libs dbus-1)

## For JSON
CFLAGS_ALL += -I "ccan/json/"
SRC += ccan/json/json.c

## For Web UI
ifneq ($(UI_WEB),0)
	SRC += ui_web.c
	CFLAGS_ALL += -DDAHSEE_UI_WEB=$(UI_WEB)
	LDFLAGS += $$(pkg-config --libs libmicrohttpd)
endif

SRC += dahsee.c
OBJ = ${SRC:.c=.o}

##==============================================================================
## Rules
##==============================================================================

all: options ${BIN}

options:
	@echo ${BIN} build options:
	@echo "CFLAGS  = ${CFLAGS_ALL}"
	@echo "LDFLAGS = ${LDFLAGS_ALL}"
	@echo "CC      = ${CC}"
	@echo "UI_WEB  = ${UI_WEB}"

${BIN}: ${OBJ}
	@${CC} ${LDFLAGS_ALL} $^ -o $@

%.o: %.c
	@${CC} ${CFLAGS_ALL} $< -c -o $@

clean:
	@rm -f ${BIN} ${OBJ}

.PHONY=all options clean

