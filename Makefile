VERSION = 0.2
TARGET  = simplewc

CC = gcc
MY_CFLAGS = $(CFLAGS) -g -Wall -DVERSION=\"$(VERSION)\" \
   -DWLR_USE_UNSTABLE -DXWAYLAND\
   $(shell pkg-config --cflags wlroots) \
   $(shell pkg-config --cflags xcb)
MY_LFLAGS = $(LDFLAGS) -lwayland-server -lxkbcommon\
   $(shell pkg-config --libs wlroots) \
   $(shell pkg-config --libs xcb)

WL_PROTOCOLS=$(shell pkg-config --variable=pkgdatadir wayland-protocols)
WL_SCANNER=$(shell pkg-config --variable=wayland_scanner wayland-scanner)

SOURCES = src/client.c src/action.c src/config.c src/layer.c src/server.c src/ipc.c src/input.c \
			 src/dwl-ipc-unstable-v2-protocol.c main.c
HEADERS = include/client.h include/action.h include/globals.h include/layer.h include/server.h include/ipc.h include/input.h \
			 include/wlr-layer-shell-unstable-v1-protocol.h include/xdg-shell-protocol.h include/dwl-ipc-unstable-v2-protocol.h \
			 include/wlr-output-power-management-unstable-v1-protocol.h
OBJECTS = $(addprefix obj/, $(notdir $(SOURCES:.c=.o)))

CRED     = "\\033[31m"
CGREEN   = "\\033[32m"
CYELLOW  = "\\033[33m"
CBLUE    = "\\033[34m"
CPURPLE  = "\\033[35m"
CRESET   = "\\033[0m"

PREFIX = /usr/local
DATADIR = $(PREFIX)/share
ETCDIR = /etc

#-------------------------------------------------------------------------
all: obj/ $(TARGET) simplewc-msg 

include/wlr-output-power-management-unstable-v1-protocol.h:
	@echo -e " [ $(CGREEN)WL$(CRESET) ] Creating $@"
	@$(WL_SCANNER) server-header protocols/wlr-output-power-management-unstable-v1.xml $@

include/wlr-layer-shell-unstable-v1-protocol.h:
	@echo -e " [ $(CGREEN)WL$(CRESET) ] Creating $@"
	@$(WL_SCANNER) server-header protocols/wlr-layer-shell-unstable-v1.xml $@

include/xdg-shell-protocol.h:
	@echo -e " [ $(CGREEN)WL$(CRESET) ] Creating $@"
	@$(WL_SCANNER) server-header $(WL_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

include/dwl-ipc-unstable-v2-protocol.h:
	@echo -e " [ $(CGREEN)WL$(CRESET) ] Creating $@"
	@$(WL_SCANNER) server-header protocols/dwl-ipc-unstable-v2.xml $@

src/dwl-ipc-unstable-v2-protocol.c:
	@echo -e " [ $(CGREEN)WL$(CRESET) ] Creating $@"
	@$(WL_SCANNER) private-code protocols/dwl-ipc-unstable-v2.xml $@

util/dwl-ipc-unstable-v2-protocol.h:
	@echo -e " [ $(CGREEN)WL$(CRESET) ] Creating $@"
	@$(WL_SCANNER) client-header protocols/dwl-ipc-unstable-v2.xml $@

util/dwl-ipc-unstable-v2-protocol.c:
	@echo -e " [ $(CGREEN)WL$(CRESET) ] Creating $@"
	@$(WL_SCANNER) private-code protocols/dwl-ipc-unstable-v2.xml $@

#-----
obj/%.o: %.c $(HEADERS) 
	@echo -e " [ $(CGREEN)CC$(CRESET) ] $< -> $@"
	@$(CC) $(MY_CFLAGS) -Iinclude -o $@ -c $<

obj/%.o: src/%.c $(HEADERS) 
	@echo -e " [ $(CGREEN)CC$(CRESET) ] $< -> $@"
	@$(CC) $(MY_CFLAGS) -Iinclude -o $@ -c $<

obj/: 
	@echo -e " [ $(CYELLOW)MKDIR$(CRESET) ] obj"
	@mkdir -p obj

$(TARGET): $(OBJECTS) 
	@echo -e " [ $(CPURPLE)BIN$(CRESET) ] $(TARGET)"
	@$(CC) -o $@ $(OBJECTS) $(MY_LFLAGS)

simplewc-msg: util/simplewc-msg.c util/dwl-ipc-unstable-v2-protocol.h util/dwl-ipc-unstable-v2-protocol.c
	@echo -e " [ $(CPURPLE)BIN$(CRESET) ] $@"
	@$(CC) -o $@ $^ -Iinclude -lwayland-client

clean:
	@echo -e " [ $(CRED)RM$(CRESET) ] $(TARGET)"
	@rm -f $(TARGET)
	@echo -e " [ $(CRED)RM$(CRESET) ] simplewc-msg"
	@rm -f simplewc-msg
	@echo -e " [ $(CRED)RM$(CRESET) ] $(OBJECTS)"
	@rm -f $(OBJECTS)
	@echo -e " [ $(CRED)RM$(CRESET) ] wlr-layer-shell-unstable-v1-protocol.h xdg-shell-protocol.h wlr-output-power-manangement-unstable-v1-protocol.h"
	@rm -f include/wlr-layer-shell-unstable-v1-protocol.h include/xdg-shell-protocol.h include/wlr-output-power-management-unstable-v1-protocol.h
	@echo -e " [ $(CRED)RM$(CRESET) ] src/dwl-ipc-unstable-v2-protocol.c include/dwl-ipc-unstable-v2-protocol.h"
	@rm -f src/dwl-ipc-unstable-v2-protocol.c include/dwl-ipc-unstable-v2-protocol.h
	@echo -e " [ $(CRED)RM$(CRESET) ] util/dwl-ipc-unstable-v2-protocol.c util/dwl-ipc-unstable-v2-protocol.h"
	@rm -f util/dwl-ipc-unstable-v2-protocol.c util/dwl-ipc-unstable-v2-protocol.h
	@echo

install: simplewc simplewc-msg
	@echo -e " [ $(CBLUE)INSTALL$(CRESET) ] $(TARGET) \uf061 $(DESTDIR)$(PREFIX)/bin"
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@cp -f $(TARGET) $(DESTDIR)$(PREFIX)/bin
	@chmod 755 $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	@echo -e " [ $(CBLUE)INSTALL$(CRESET) ] simplewc-msg \uf061 $(DESTDIR)$(PREFIX)/bin"
	@cp -f simplewc-msg $(DESTDIR)$(PREFIX)/bin
	@chmod 755 $(DESTDIR)$(PREFIX)/bin/simplewc-msg
	@echo -e " [ $(CBLUE)INSTALL$(CRESET) ] $(TARGET).desktop \uf061 $(DESTDIR)$(DATADIR)/wayland-sessions"
	@mkdir -p $(DESTDIR)$(DATADIR)/wayland-sessions
	@cp -f $(TARGET).desktop $(DESTDIR)$(DATADIR)/wayland-sessions
	@chmod 644 $(DESTDIR)$(DATADIR)/wayland-sessions/$(TARGET).desktop
	@echo -e " [ $(CBLUE)INSTALL$(CRESET) ] configrc \uf061 $(DESTDIR)$(ETCDIR)/simplewc"
	@mkdir -p $(DESTDIR)$(ETCDIR)/simplewc
	@cp -f config/configrc $(DESTDIR)$(ETCDIR)/simplewc/configrc
	@chmod 644 $(DESTDIR)$(ETCDIR)/simplewc/configrc

uninstall:
	@echo -e " [ $(CRED)UNINSTALL$(CRESET) ] $(TARGET)"
	@rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	@echo -e " [ $(CRED)UNINSTALL$(CRESET) ] $(TARGET)-msg"
	@rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)-msg
	@echo -e " [ $(CRED)UNINSTALL$(CRESET) ] $(TARGET).desktop"
	@rm -f $(DESTDIR)$(DATADIR)/wayland-sessions/$(TARGET).desktop
	@echo -e " [ $(CRED)UNINSTALL$(CRESET) ] configrc"
	@rm -f $(DESTDIR)$(ETCDIR)/simplewc/configrc

info:
	@echo $(TARGET) build options:
	@echo "CC      = $(CC)"
	@echo "SOURCES = $(SOURCES)"
	@echo "HEADERS = $(HEADERS)"
	@echo "OBJECTS = $(OBJECTS)"
	@echo "CFLAGS  = $(MY_CFLAGS)"
	@echo "LFLAGS  = $(MY_LFLAGS)"

.PHONY: obj all clean info
