################################################################################
# Dahsee makefile
# Date 2012-06-28
################################################################################
## FIXME: does not work?
.PHONY=test

include config.mk

# CFLAGS+=-Wall -Wextra -Wshadow -Winline -pedantic
CFLAGS+=-Wall -Wextra -Wshadow -Winline -pedantic -std=c99

# For use of getopt() in -pedantic
CFLAGS+=-D_XOPEN_SOURCE=500

LDFLAGS+=-lpcap -ljson

CFLAGS+=$$(pkg-config --cflags dbus-1)
LDFLAGS+=$$(pkg-config --libs dbus-1)


## Projects options
# USE_GLIB=false
# ifeq ($(USE_GLIB),true)
# 	CFLAGS+=$$(pkg-config --cflags dbus-glib-1)
# 	LDFLAGS+=$$(pkg-config --libs dbus-glib-1)
# else
# 	CFLAGS+=$$(pkg-config --cflags dbus-1)
# 	LDFLAGS+=$$(pkg-config --libs dbus-1)
# endif

BIN=dahsee

all: $(BIN)

$(BIN): $(BIN).c
	@$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS)

clean:
	@$(RM) $(BIN)

