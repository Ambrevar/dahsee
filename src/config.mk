## Dahsee version
VERSION = "0.1"
AUTHOR = "Pierre Neidhardt <ambrevar [at] gmail [dot] com>"

## Customize below to fit your system

CFLAGS+=-DVERSION=\"${VERSION}\"
CFLAGS+=-g3 -O0
# CFLAGS+=-Os

## Paths
PREFIX = /usr/local

## Tools
CC=gcc
RM=rm -f
