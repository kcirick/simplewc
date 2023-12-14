#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <wlr/util/log.h>

#define MAX_TAGS 10

#define XDG_SHELL_VERSION (3)
#define LAYER_SHELL_VERSION (4)

#define N_LAYER_SHELL_LAYERS 4

// macros
#define LISTEN(E, L, H) wl_signal_add((E), ((L)->notify = (H), (L)))
#define LENGTH(X)       (sizeof X / sizeof X[0])

//--- enums -----
enum MessageType { DEBUG, INFO, WARNING, ERROR, NMSG };
enum BorderColours { FOCUSED, UNFOCUSED, URGENT, MARKED, FIXED, NBORDERCOL };
enum KeyFunctions { SPAWN, QUIT, TAG, CLIENT, NFUNC };
enum MouseContext { CONTEXT_ROOT, CONTEXT_CLIENT, NCONTEXT};
enum cursor_mode { CURSOR_PASSTHROUGH, CURSOR_MOVE, CURSOR_RESIZE };
enum client_type { XDG_SHELL_CLIENT, LAYER_SHELL_CLIENT, XWL_MANAGED_CLIENT, XWL_UNMANAGED_CLIENT };
enum inputType {INPUT_POINTER, INPUT_KEYBOARD, INPUT_MISC };

struct simple_config {
   int n_tags;
   char tag_names[MAX_TAGS][32];
   int border_width;
   bool sloppy_focus;
   int moveresize_step;

   float background_colour[4];
   float border_colour[NBORDERCOL][4];

   struct wl_list key_bindings;
   struct wl_list mouse_bindings;
};

struct keymap {
   uint32_t mask;
   xkb_keysym_t keysym;
   int keyfn;
   char argument[32];
   
   struct wl_list link;
};

struct mousemap {
   uint32_t mask;
   uint32_t button;
   int context;
   char argument[32];
   
   struct wl_list link;
};

//--- functions in config.c -----
struct simple_config * readConfiguration(char*);

//--- functions in main.c -----
void say(int, const char*, ...);
void spawn(char*);

#endif

