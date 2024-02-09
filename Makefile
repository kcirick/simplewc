VERSION = 0.2
TARGET  = simplewc

CC = gcc
PREFIX = /usr/local
#0 - Don't use XWAYLAND / 1 - Use XWAYLAND
USE_XWAYLAND = 1

MY_CFLAGS = $(CFLAGS) -g -Wall -DVERSION=\"$(VERSION)\" -DWLR_USE_UNSTABLE \
   $(shell pkg-config --cflags wlroots)
MY_LFLAGS = $(LDFLAGS) -lwayland-server -lxkbcommon\
   $(shell pkg-config --libs wlroots)
ifeq ($(USE_XWAYLAND), 1)
	MY_CFLAGS += -DXWAYLAND $(shell pkg-config --cflags xcb)
	MY_LFLAGS += $(shell pkg-config --libs xcb)
endif

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

#-----
obj/: 
	@echo -e " [ $(CYELLOW)MKDIR$(CRESET) ] obj directory ..."
	@mkdir -p obj

obj/%.o: %.c $(HEADERS) 
	@echo -e " [ $(CGREEN)CC$(CRESET) ] $< \uf061 $@"
	@$(CC) $(MY_CFLAGS) -Iinclude -o $@ -c $<

obj/%.o: src/%.c $(HEADERS) 
	@echo -e " [ $(CGREEN)CC$(CRESET) ] $< \uf061 $@"
	@$(CC) $(MY_CFLAGS) -Iinclude -o $@ -c $<

obj/%.o: util/%.c util/dwl-ipc-unstable-v2-protocol.h
	@echo -e " [ $(CGREEN)CC$(CRESET) ] $< \uf061 $@"
	@$(CC) -g -Wall -Iutil -o $@ -c $<

$(TARGET): $(OBJECTS) 
	@echo -e " [ $(CPURPLE)BIN$(CRESET) ] $(TARGET)"
	@$(CC) -o $@ $(OBJECTS) $(MY_LFLAGS)

simplewc-msg: obj/simplewc-msg.o obj/dwl-ipc-unstable-v2-protocol.o
	@echo -e " [ $(CPURPLE)BIN$(CRESET) ] $@"
	@$(CC) -o $@ $^ -lwayland-client

#-----
install: simplewc simplewc-msg
	@echo -e " [ $(CBLUE)INST$(CRESET) ] $(TARGET) \uf061 $(DESTDIR)$(PREFIX)/bin"
	@install -Dm755 $(TARGET) $(DESTDIR)$(PREFIX)/bin
	@echo -e " [ $(CBLUE)INST$(CRESET) ] simplewc-msg \uf061 $(DESTDIR)$(PREFIX)/bin"
	@install -Dm755 simplewc-msg $(DESTDIR)$(PREFIX)/bin
	@echo -e " [ $(CBLUE)INST$(CRESET) ] $(TARGET).desktop \uf061 $(DESTDIR)$(PREFIX)/share/wayland-sessions"
	@install -Dm644 $(TARGET).desktop $(DESTDIR)$(PREFIX)/share/wayland-sessions

uninstall:
	@echo -e " [ $(CRED)UNINST$(CRESET) ] $(TARGET)"
	@rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	@echo -e " [ $(CRED)UNINST$(CRESET) ] simplewc-msg"
	@rm -f $(DESTDIR)$(PREFIX)/bin/simplewc-msg
	@echo -e " [ $(CRED)UNINST$(CRESET) ] $(TARGET).desktop"
	@rm -f $(DESTDIR)$(PREFIX)/share/wayland-sessions/$(TARGET).desktop

clean:
	@echo -e " [ $(CRED)RM$(CRESET) ] $(TARGET)"
	@rm -f $(TARGET)
	@echo -e " [ $(CRED)RM$(CRESET) ] simplewc-msg"
	@rm -f simplewc-msg
	@echo -e " [ $(CRED)RM$(CRESET) ] Object files ..."
	@rm -f $(OBJECTS) obj/simplewc-msg.o
	@echo -e " [ $(CRED)RM$(CRESET) ] Protocol header/c files ..." 
	@rm -f include/wlr-layer-shell-unstable-v1-protocol.h include/xdg-shell-protocol.h include/wlr-output-power-management-unstable-v1-protocol.h
	@rm -f src/dwl-ipc-unstable-v2-protocol.c include/dwl-ipc-unstable-v2-protocol.h util/dwl-ipc-unstable-v2-protocol.h

info:
	@echo $(TARGET) build options:
	@echo "PREFIX  = $(PREFIX)"
	@echo "CC      = $(CC)"
	@echo "SOURCES = $(SOURCES)"
	@echo "HEADERS = $(HEADERS)"
	@echo "OBJECTS = $(OBJECTS)"
	@echo "CFLAGS  = $(MY_CFLAGS)"
	@echo "LFLAGS  = $(MY_LFLAGS)"

