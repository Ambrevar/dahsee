################################################################################
## Dahsee version
################################################################################
APPNAME = "Dahsee"
VERSION = "0.1"
AUTHOR = "Pierre Neidhardt"
# MAIL <ambrevar [at] gmail [dot] com>"
YEAR = "2012"

## UI Support
## Use '0' to turn off, anything else to turn on.
UI_WEB ?= 0

## Customize below to fit your system
# CFLAGS ?= -g3 -O0
CFLAGS ?= -Os

## Paths
PREFIX ?= /usr/local

##==============================================================================
## End of user configuration
##==============================================================================
CC=gcc
RM=rm -f

CFLAGS += -DAPPNAME=\"${APPNAME}\"
CFLAGS += -DVERSION=\"${VERSION}\"
CFLAGS += -DAUTHOR=\"${AUTHOR}\"
CFLAGS += -DYEAR=\"${YEAR}\"

CFLAGS += -DDAHSEE_UI_WEB=$(UI_WEB)

##==============================================================================
## End of configuration
##==============================================================================
