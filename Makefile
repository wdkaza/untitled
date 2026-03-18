PKG_CONFIG ?= pkg-config
PKGS = wayland-server xkbcommon xwayland

# TODO : remake makefile myself, this was done by chatgpt bcs i couldnt compile for some reason, maybe switch to meson
# another comment,: holy moly this makefile is the worst, i will not make the repo public until it looks good and made by me(comment to whoever could be reading my old commits :D),
# lesson learned dont trust chatgpt with anything
WLROOTS_BUILD = /home/wdkaza/packages/wlroots/build
CFLAGS_PKG_CONFIG = $(shell $(PKG_CONFIG) --cflags $(PKGS))
CFLAGS_PKG_CONFIG += -I/home/wdkaza/packages/wlroots/include
CFLAGS_PKG_CONFIG += -I$(WLROOTS_BUILD)
CFLAGS_PKG_CONFIG += -I$(WLROOTS_BUILD)/protocol
CFLAGS_PKG_CONFIG += -I/usr/include/pixman-1
CFLAGS_PKG_CONFIG += -I/home/wdkaza/packages/wlroots/build/include
LIBS = $(shell $(PKG_CONFIG) --libs $(PKGS) xcb-icccm) -lGLESv2 -lwlroots-0.20 -lpixman-1
LDFLAGS += -L$(WLROOTS_BUILD) -Wl,-rpath,$(WLROOTS_BUILD)

ASAN ?= 1

CFLAGS += -g -Werror -DWLR_USE_UNSTABLE -DXWAYLAND $(CFLAGS_PKG_CONFIG)
CFLAGS += -Isrc

ifeq ($(ASAN),1)
    CFLAGS += -fsanitize=address
    LDFLAGS += -fsanitize=address
endif

OBJS = main.o src/renderer.o

all: main

main: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) $(LIBS) -Wl,-rpath,/usr/local/lib -o $@

main.o: main.c
	$(CC) -c $< $(CFLAGS) -o $@

src/renderer.o: src/renderer.c src/renderer.h
	$(CC) -c $< $(CFLAGS) -o $@

clean:
	rm -f main $(OBJS)

.PHONY: all clean
