VERSION = 0.1
TARGET  = simpleway

CC = gcc
MY_CFLAGS = $(CFLAGS) -g -Wall -DVERSION=\"$(VERSION)\" \
   -DWLR_USE_UNSTABLE -DXWAYLAND \
   $(shell pkg-config --cflags wlroots)
MY_LFLAGS = $(LDFLAGS) -lwayland-server -lxkbcommon\
   $(shell pkg-config --libs wlroots)

WL_PROTOCOLS=$(shell pkg-config --variable=pkgdatadir wayland-protocols)
WL_SCANNER=$(shell pkg-config --variable=wayland_scanner wayland-scanner)

SOURCES = src/action.c src/config.c src/layer.c src/seat.c src/server.c \
   src/xdg.c src/xwayland.c \
   wlr-layer-shell-unstable-v1-protocol.c xdg-shell-protocol.c main.c
HEADERS = include/action.h include/globals.h include/layer.h include/seat.h include/server.h \
   include/xdg.h include/xwayland.h \
   wlr-layer-shell-unstable-v1-protocol.h xdg-shell-protocol.h
OBJECTS = $(addprefix obj/, $(notdir $(SOURCES:.c=.o)))

CRED     = "\\033[31m"
CGREEN   = "\\033[32m"
CYELLOW  = "\\033[33m"
CPURPLE  = "\\033[35m"
CRESET   = "\\033[0m"

#-------------------------------------------------------------------------
all: $(TARGET) 

wlr-layer-shell-unstable-v1-protocol.h:
	@echo -e " [ $(CGREEN)WL$(CRESET) ] Creating $@"
	@$(WL_SCANNER) server-header proto/wlr-layer-shell-unstable-v1.xml $@

wlr-layer-shell-unstable-v1-protocol.c: wlr-layer-shell-unstable-v1-protocol.h
	@echo -e " [ $(CGREEN)WL$(CRESET) ] $< -> $@"
	@$(WL_SCANNER) private-code proto/wlr-layer-shell-unstable-v1.xml $@

xdg-shell-protocol.h:
	@echo -e " [ $(CGREEN)WL$(CRESET) ] Creating $@"
	@$(WL_SCANNER) server-header $(WL_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

xdg-shell-protocol.c: xdg-shell-protocol.h
	@echo -e " [ $(CGREEN)WL$(CRESET) ] $< -> $@"
	@$(WL_SCANNER) private-code $(WL_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

obj/%.o: %.c
	@echo -e " [ $(CGREEN)CC$(CRESET) ] $< -> $@"
	@$(CC) $(MY_CFLAGS) -I. -Iinclude -o $@ -c $<

obj/%.o: src/%.c 
	@echo -e " [ $(CGREEN)CC$(CRESET) ] $< -> $@"
	@$(CC) $(MY_CFLAGS) -I. -Iinclude -o $@ -c $<

$(OBJECTS): $(HEADERS)

$(TARGET): $(OBJECTS) $(SOURCES) 
	@echo -e " [ $(CPURPLE)LD$(CRESET) ] $(TARGET)"
	@$(CC) -o $@ $(OBJECTS) $(MY_LFLAGS)

clean:
	@echo -e " [ $(CRED)RM$(CRESET) ] $(TARGET)"
	@rm -f $(TARGET)
	@echo -e " [ $(CRED)RM$(CRESET) ] $(OBJECTS)"
	@rm -f $(OBJECTS)
	@echo -e " [ $(CRED)RM$(CRESET) ] wlr-layer-shell-unstable-v1-protocol.{c,h} xdg-shell-protocol.{c,h}"
	@rm -f wlr-layer-shell-unstable-v1-protocol.{c,h} xdg-shell-protocol.{c,h}
	@echo

info:
	@echo $(TARGET) build options:
	@echo "CC      = $(CC)"
	@echo "SOURCES = $(SOURCES)"
	@echo "HEADERS = $(HEADERS)"
	@echo "OBJECTS = $(OBJECTS)"
	@echo "CFLAGS  = $(MY_CFLAGS)"
	@echo "LFLAGS  = $(MY_LFLAGS)"

.PHONY: all clean info


