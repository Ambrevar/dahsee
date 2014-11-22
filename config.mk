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

## Listening port of the daemon.
# PORT ?= 7117

## Optional compilation flags.
CFLAGS ?= -pedantic -std=c11 -Wall -Wextra -Wshadow

## END OF USER SETTINGS
