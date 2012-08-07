## Dahsee version
APPNAME = "Dahsee"
VERSION = "0.1"
AUTHOR = "Pierre Neidhardt"
# MAIL <ambrevar [at] gmail [dot] com>"
YEAR = "2012"

## UI Support
UI_WEB ?= -DDAHSEE_UI_WEB

CFLAGS += $(UI_WEB)
CFLAGS += -DAPPNAME=\"${APPNAME}\"
CFLAGS += -DVERSION=\"${VERSION}\"
CFLAGS += -DAUTHOR=\"${AUTHOR}\"
CFLAGS += -DYEAR=\"${YEAR}\"


## Customize below to fit your system
CFLAGS+=-g3 -O0
# CFLAGS+=-Os

## Paths
PREFIX = /usr/local

## Tools
CC=gcc
RM=rm -f
