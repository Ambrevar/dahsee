ROOT ?= ..
include ${ROOT}/config.mk

CPPFLAGS += -D_POSIX_C_SOURCE=200809L
CFLAGS += `pkg-config --cflags dbus-1`
LDLIBS += `pkg-config --libs dbus-1`

cmdnames = signal-get signal-send dbus-debug-check list methodcall-get methodcall-send

all: ${cmdnames}

.PHONY: debug
debug:
	CFLAGS+="-g3 -O0 -DDEBUG=9" ${MAKE}

.PHONY: clean
clean:
	rm -f ${cmdnames} *.d *.o

## Generate prerequisites automatically. GNU Make only.
## The 'awk' part is used to add the .d file itself to the target, so that it
## gets updated on changes. The -MQ option is to add full path to object files
## in subfolders. (-MM strips parent directories.)
%.d: %.c
	${CC} -MM -MQ ${<:.c=.o} ${CPPFLAGS} $< | awk -v stem=$* -v target=$@ '{gsub (stem ".o[ :]*", stem ".o " target " : ")} 1' > $@

sources = $(wildcard *.c)
deps = ${sources:.c=.d}
-include ${deps}
