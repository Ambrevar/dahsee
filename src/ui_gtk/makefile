SRC = ui_gtk.c

BIN = dahsee-gtk

CFLAGS += -Wall -Wextra -Wshadow
CFLAGS += $$(pkg-config --cflags gtk+-2.0)
CFLAGS += -Os

LDFLAGS += $$(pkg-config --libs gtk+-2.0)

all:
	gcc $(CFLAGS) $(SRC) -o  $(BIN) $(LDFLAGS)
