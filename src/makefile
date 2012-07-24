################################################################################
# Dahsee makefile
# Date 2012-06-28
################################################################################
## FIXME: does not work.
.PHONY=test

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

BIN=dahsee

all: dahsee

dahsee: dahsee.c
	@$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS)

clean:
	@$(RM) $(BIN)

