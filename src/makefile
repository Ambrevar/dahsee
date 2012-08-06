################################################################################
# Dahsee makefile
# Date 2012-06-28
################################################################################
## FIXME: does not work?
.PHONY=test

include config.mk

## TODO: must make json.c following C99 instead of C11?
# CFLAGS+=-Wall -Wextra -Wshadow -Winline
CFLAGS+=-Wall -Wextra -Wshadow -Winline -pedantic -std=c11

## For use of getopt() with -pedantic.
CFLAGS+=-D_XOPEN_SOURCE=700
# CFLAGS+=-D_ISOC11_SOURCE

## D-Bus
CFLAGS+=$$(pkg-config --cflags dbus-1)
LDFLAGS+=$$(pkg-config --libs dbus-1)

## For JSON
CFLAGS+=-I "ccan/json/"
SRC+=ccan/json/json.c

## For microhttpd 
LDFLAGS+=$$(pkg-config --libs libmicrohttpd)

BIN=dahsee
SRC+=dahsee.c
OBJ=$(SRC:.c=.o)

all: $(BIN)

$(BIN): $(OBJ)
	@$(CC) $(LDFLAGS) $^ -o $@

%.o: %.c
	@$(CC) $(CFLAGS) $< -c -o $@

clean:
	@$(RM) $(BIN) $(OBJ)


################################################################################
## OLD

## Projects options
# USE_GLIB=false
# ifeq ($(USE_GLIB),true)
# 	CFLAGS+=$$(pkg-config --cflags dbus-glib-1)
# 	LDFLAGS+=$$(pkg-config --libs dbus-glib-1)
# else
# 	CFLAGS+=$$(pkg-config --cflags dbus-1)
# 	LDFLAGS+=$$(pkg-config --libs dbus-1)
# endif

# LDFLAGS+=-lpcap
# LDFLAGS+=-ljson ## For use with Jansson.
