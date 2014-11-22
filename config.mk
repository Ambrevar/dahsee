## This file is included by all (sub-)makefiles.

## Properties.
appname = Dahsee
authors = Pierre Neidhardt
cmdname = dahsee
url = http://bitbucket.org/ambrevar/dahsee
version = 1.0
year = 2014

## Folders.
srcdir = src
docsrcdir = doc
testdir = tests

## USER SETTINGS

## TODO: change behaviour of this.
## UI Support
## Use '0' to turn off, anything else to turn on.
UI_WEB ?= 1

## Optional compilation flags.
CFLAGS ?= -pedantic -std=c11 -Wall -Wextra -Wshadow

## END OF USER SETTINGS
