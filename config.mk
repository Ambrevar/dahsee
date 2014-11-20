##==============================================================================
## Dahsee make configuration
##==============================================================================

APPNAME = "Dahsee"
BIN = dahsee
VERSION = 0.1
AUTHOR = "Pierre Neidhardt"
MANPAGE = "DAHSEE"
MANSECTION = "1"
# MAIL <ambrevar [at] gmail [dot] com>"
YEAR = "2012"

## UI Support
## Use '0' to turn off, anything else to turn on.
UI_WEB ?= 1

## Paths
PREFIX ?= /usr/local
BINDIR ?= ${PREFIX}/bin
DATADIR ?= ${PREFIX}/share
MANDIR ?= ${DATADIR}/man
MAN1DIR ?= ${MANDIR}/man1

## Tools
CC=gcc

## Customize below to fit your system
# CFLAGS ?= -g3 -O0
CFLAGS ?= -Os
LDFLAGS ?= -s
