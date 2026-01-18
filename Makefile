PKG_CONFIG ?= pkg-config
PKGS = wlroots-0.20 wayland-server xkbcommon xwayland

# TODO : remake makefile myself, this was done by chatgpt bcs i couldnt compile for some reason, maybe switch to meson
CFLAGS_PKG_CONFIG = $(shell $(PKG_CONFIG) --cflags $(PKGS))
CFLAGS_PKG_CONFIG += -I/usr/src/debug/wlroots-asan-git/build/protocol
LIBS = $(shell $(PKG_CONFIG) --libs $(PKGS))

ASAN ?= 1

CFLAGS += -g -Werror -DWLR_USE_UNSTABLE -DXWAYLAND $(CFLAGS_PKG_CONFIG)
LDFLAGS :=

ifeq ($(ASAN),1)
    CFLAGS += -fsanitize=address
    LDFLAGS += -fsanitize=address
endif

OBJS = main.o

all: main

main: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) $(LIBS) -Wl,-rpath,/usr/local/lib -o $@

main.o: main.c
	$(CC) -c $< $(CFLAGS) -o $@

clean:
	rm -f main $(OBJS)

.PHONY: all clean
