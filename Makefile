VERSION = 0.2
TARGET  = swc 

CC = gcc
MY_CFLAGS = $(CFLAGS) -g -Wall -DVERSION=\"$(VERSION)\" \
   -DWLR_USE_UNSTABLE -DXWAYLAND \
   $(shell pkg-config --cflags wlroots)
MY_LFLAGS = $(LDFLAGS) -lwayland-server -lxkbcommon\
   $(shell pkg-config --libs wlroots)

WL_PROTOCOLS=$(shell pkg-config --variable=pkgdatadir wayland-protocols)
WL_SCANNER=$(shell pkg-config --variable=wayland_scanner wayland-scanner)

SOURCES = src/client.c src/action.c src/config.c src/layer.c src/server.c src/ipc.c \
			 src/dwl-ipc-unstable-v2-protocol.c main.c
HEADERS = include/client.h include/action.h include/globals.h include/layer.h include/server.h include/ipc.h \
			 include/wlr-layer-shell-unstable-v1-protocol.h include/xdg-shell-protocol.h include/dwl-ipc-unstable-v2-protocol.h \
			 include/wlr-output-power-management-unstable-v1-protocol.h
OBJECTS = $(addprefix obj/, $(notdir $(SOURCES:.c=.o)))

CRED     = "\\033[31m"
CGREEN   = "\\033[32m"
CYELLOW  = "\\033[33m"
CPURPLE  = "\\033[35m"
CRESET   = "\\033[0m"

#-------------------------------------------------------------------------
all: objdir $(TARGET) swc-msg 

include/wlr-output-power-management-unstable-v1-protocol.h:
	@echo " [ $(CGREEN)WL$(CRESET) ] Creating $@"
	@$(WL_SCANNER) server-header protocols/wlr-output-power-management-unstable-v1.xml $@

include/wlr-layer-shell-unstable-v1-protocol.h:
	@echo " [ $(CGREEN)WL$(CRESET) ] Creating $@"
	@$(WL_SCANNER) server-header protocols/wlr-layer-shell-unstable-v1.xml $@

include/xdg-shell-protocol.h:
	@echo " [ $(CGREEN)WL$(CRESET) ] Creating $@"
	@$(WL_SCANNER) server-header $(WL_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

include/dwl-ipc-unstable-v2-protocol.h:
	@echo " [ $(CGREEN)WL$(CRESET) ] Creating $@"
	@$(WL_SCANNER) server-header protocols/dwl-ipc-unstable-v2.xml $@

src/dwl-ipc-unstable-v2-protocol.c:
	@echo " [ $(CGREEN)WL$(CRESET) ] Creating $@"
	@$(WL_SCANNER) private-code protocols/dwl-ipc-unstable-v2.xml $@

util/dwl-ipc-unstable-v2-protocol.h:
	@echo " [ $(CGREEN)WL$(CRESET) ] Creating $@"
	@$(WL_SCANNER) client-header protocols/dwl-ipc-unstable-v2.xml $@

util/dwl-ipc-unstable-v2-protocol.c:
	@echo " [ $(CGREEN)WL$(CRESET) ] Creating $@"
	@$(WL_SCANNER) private-code protocols/dwl-ipc-unstable-v2.xml $@

#-----
obj/%.o: %.c $(HEADERS) 
	@echo " [ $(CGREEN)CC$(CRESET) ] $< -> $@"
	@$(CC) $(MY_CFLAGS) -Iinclude -o $@ -c $<

obj/%.o: src/%.c $(HEADERS) 
	@echo " [ $(CGREEN)CC$(CRESET) ] $< -> $@"
	@$(CC) $(MY_CFLAGS) -Iinclude -o $@ -c $<

objdir:
	@echo " [ $(CYELLOW)MKDIR$(CRESET) ] obj"
	@mkdir -p obj

$(TARGET): $(OBJECTS) 
	@echo " [ $(CPURPLE)BIN$(CRESET) ] $(TARGET)"
	@$(CC) -o $@ $(OBJECTS) $(MY_LFLAGS)

swc-msg: util/swc-msg.c util/dwl-ipc-unstable-v2-protocol.h util/dwl-ipc-unstable-v2-protocol.c
	@echo " [ $(CPURPLE)BIN$(CRESET) ] $@"
	@$(CC) -o $@ $^ -Iinclude -lwayland-client

clean:
	@echo " [ $(CRED)RM$(CRESET) ] $(TARGET)"
	@rm -f $(TARGET)
	@echo " [ $(CRED)RM$(CRESET) ] swc-msg"
	@rm -f swc-msg
	@echo " [ $(CRED)RM$(CRESET) ] $(OBJECTS)"
	@rm -f $(OBJECTS)
	@echo " [ $(CRED)RM$(CRESET) ] wlr-layer-shell-unstable-v1-protocol.h xdg-shell-protocol.h wlr-output-power-manangement-unstable-v1-protocol.h"
	@rm -f include/wlr-layer-shell-unstable-v1-protocol.h include/xdg-shell-protocol.h include/wlr-output-power-management-unstable-v1-protocol.h
	@echo " [ $(CRED)RM$(CRESET) ] src/dwl-ipc-unstable-v2-protocol.c include/dwl-ipc-unstable-v2-protocol.h"
	@rm -f src/dwl-ipc-unstable-v2-protocol.c include/dwl-ipc-unstable-v2-protocol.h
	@echo " [ $(CRED)RM$(CRESET) ] util/dwl-ipc-unstable-v2-protocol.c util/dwl-ipc-unstable-v2-protocol.h"
	@rm -f util/dwl-ipc-unstable-v2-protocol.c util/dwl-ipc-unstable-v2-protocol.h
	@echo

info:
	@echo $(TARGET) build options:
	@echo "CC      = $(CC)"
	@echo "SOURCES = $(SOURCES)"
	@echo "HEADERS = $(HEADERS)"
	@echo "OBJECTS = $(OBJECTS)"
	@echo "CFLAGS  = $(MY_CFLAGS)"
	@echo "LFLAGS  = $(MY_LFLAGS)"

.PHONY: obj all clean info
