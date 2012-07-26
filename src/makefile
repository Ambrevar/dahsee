################################################################################
# Dahsee makefile
# Date 2012-06-28
################################################################################
## FIXME: does not work?
.PHONY=test

include config.mk

## TODO: must make json.c following C99.
CFLAGS+=-Wall -Wextra -Wshadow -Winline
# CFLAGS+=-Wall -Wextra -Wshadow -Winline -pedantic -std=c99

# For use of getopt() in -pedantic
CFLAGS+=-D_XOPEN_SOURCE=500

LDFLAGS+=-lpcap -ljson

CFLAGS+=$$(pkg-config --cflags dbus-1)
LDFLAGS+=$$(pkg-config --libs dbus-1)

# For JSON
CFLAGS+=-I "ccan/json/"
# CFLAGS+=-I "."

## Projects options
# USE_GLIB=false
# ifeq ($(USE_GLIB),true)
# 	CFLAGS+=$$(pkg-config --cflags dbus-glib-1)
# 	LDFLAGS+=$$(pkg-config --libs dbus-glib-1)
# else
# 	CFLAGS+=$$(pkg-config --cflags dbus-1)
# 	LDFLAGS+=$$(pkg-config --libs dbus-1)
# endif

# CFLAGS+=ccan/json/json.c

BIN=dahsee
SRC=dahsee.c ccan/json/json.c
OBJ=$(SRC:.c=.o)

all: $(BIN)

$(BIN): $(OBJ)
	@$(CC) $(LDFLAGS) $^ -o $@

%.o: %.c
	@$(CC) $(CFLAGS) $< -c -o $@

clean:
	@$(RM) $(BIN) $(OBJ)

