################################################################################
# Dahsee makefile
# Date 2012-06-28
################################################################################

CC=gcc
SRC=dahsee.c
BIN=dahsee

USE_GLIB=false

CFLAGS+=-g -O0
CFLAGS+=-Wall -Wextra -Wshadow -Winline
# CFLAGS+=-Wdeclaration-after-statement
LDFLAGS+=-lm

ifeq ($(USE_GLIB),true)
	CFLAGS+=$$(pkg-config --cflags dbus-glib-1)
	LDFLAGS+=$$(pkg-config --libs dbus-glib-1)
else
	CFLAGS+=$$(pkg-config --cflags dbus-1)
	LDFLAGS+=$$(pkg-config --libs dbus-1)
endif

all: $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LDFLAGS)
