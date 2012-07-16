################################################################################
# Dahsee makefile
# Date 2012-06-28
################################################################################

## Projects options
USE_GLIB=false

##==============================================================================

CC=gcc
RM=rm -f

BIN=dahsee pct

CFLAGS+=-g -O0
CFLAGS+=-Wall -Wextra -Wshadow -Winline
# CFLAGS+=-Wdeclaration-after-statement
LDFLAGS+=-lm
LDFLAGS+=-lpcap

ifeq ($(USE_GLIB),true)
	CFLAGS+=$$(pkg-config --cflags dbus-glib-1)
	LDFLAGS+=$$(pkg-config --libs dbus-glib-1)
else
	CFLAGS+=$$(pkg-config --cflags dbus-1)
	LDFLAGS+=$$(pkg-config --libs dbus-1)
endif

all: dahsee pct

dahsee: dahsee.c
	@$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS)

pct: pct.c
	@$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS)

clean:
	@$(RM) $(BIN)
