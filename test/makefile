################################################################################
# Dahsee makefile
# Date 2012-06-28
################################################################################

## Projects options
USE_GLIB=false

##==============================================================================

CC=gcc
RM=rm -f

CFLAGS+=-g -O0
CFLAGS+=-Wall -Wextra -Wshadow -Winline
# CFLAGS+=-Wdeclaration-after-statement
LDFLAGS+=-lm
LDFLAGS+=-lpcap -ljson

ifeq ($(USE_GLIB),true)
	CFLAGS+=$$(pkg-config --cflags dbus-glib-1)
	LDFLAGS+=$$(pkg-config --libs dbus-glib-1)
else
	CFLAGS+=$$(pkg-config --cflags dbus-1)
	LDFLAGS+=$$(pkg-config --libs dbus-1)
endif

BIN=signal-get signal-send dbus-debug-check list methodcall-get methodcall-send

all: signal-get signal-send dbus-debug-check list methodcall-get methodcall-send

signal-get: signal-get.c
	@$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS)

signal-send: signal-send.c
	@$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS)

dbus-debug-check: dbus-debug-check.c
	@$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS)

list: list.c
	@$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS)

methodcall-get: methodcall-get.c
	@$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS)

methodcall-send: methodcall-send.c
	@$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS)

clean:
	@$(RM) $(BIN)

