ROOT ?= ..
include ${ROOT}/config.mk

CPPFLAGS += -DAUTHORS="\"${authors}\"" -DVERSION=\"${version}\" -DYEAR=\"${year}\" -DAPPNAME=\"${appname}\"
CPPFLAGS += -D_POSIX_C_SOURCE=200809L

CFLAGS += `pkg-config --cflags dbus-1`
LDLIBS += `pkg-config --libs dbus-1`

## C11 is necessary for ccan/json.
CPPFLAGS += -I "ccan/json/"
CFLAGS += -std=c11

CPPFLAGS += -DDAHSEE_UI_WEB=1
LDLIBS += `pkg-config --libs libmicrohttpd`
PORT ?= 7117
CPPFLAGS += -DPORT=${PORT}

## The recipe is the implicit rule of GNU Make. Unfortunately non-GNU Make are
## not so smart at guessing the right recipe so we need to add it here.
${cmdname}: ui_web.o ccan/json/json.o ${cmdname}.o
	${CC} ${LDFLAGS} ${TARGET_ARCH} ui_web.o ccan/json/json.o ${cmdname}.o $(LOADLIBES) $(LDLIBS) -o $@

.PHONY: debug
debug:
	CFLAGS+="-g3 -O0 -DDEBUG=9" ${MAKE}

.PHONY: clean
clean:
	rm -f ${cmdname} *.d *.o ccan/json/json.o

## Generate prerequisites automatically. GNU Make only.
## The 'awk' part is used to add the .d file itself to the target, so that it
## gets updated on changes. The -MQ option is to add full path to object files
## in subfolders. (-MM strips parent directories.)
%.d: %.c
	${CC} -MM -MQ ${<:.c=.o} ${CPPFLAGS} $< | awk -v stem=$* -v target=$@ '{gsub (stem ".o[ :]*", stem ".o " target " : ")} 1' > $@

sources = $(wildcard *.c)
deps = ${sources:.c=.d}
-include ${deps}
