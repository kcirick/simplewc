#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdio.h>
#include <stdlib.h>

#include <wayland-server-core.h>
#include <xkbcommon/xkbcommon.h>

#define MAX_TAGS 9

#define XDG_SHELL_VERSION (3)
#define LAYER_SHELL_VERSION (4)
#define COMPOSITOR_VERSION (5)
#define FRAC_SCALE_VERSION (1)
#define DWL_IPC_VERSION (2)

#define N_LAYER_SHELL_LAYERS 4

// macros
#define LISTEN(E, L, H)    wl_signal_add((E), ((L)->notify = (H), (L)))
#define LENGTH(X)          (sizeof X / sizeof X[0])
#define TAGMASK(T)         (1 << (T))
#define VISIBLEON(C, O)    ((O) && (C)->output==(O) && (C)->visible && ((C)->fixed || ((C)->tag & (O)->visible_tags)))
#define MIN(A, B)          ((A)<(B) ? (A) : (B))
#define MAX(A, B)          ((A)>(B) ? (A) : (B))

//--- enums -----
enum BorderColours   { FOCUSED, UNFOCUSED, URGENT, MARKED, FIXED, OUTLINE, NBORDERCOL };
enum KeyFunctions    { SPAWN, QUIT, TAG, CLIENT, NFUNC };
enum MouseContext    { CONTEXT_ROOT, CONTEXT_CLIENT, NCONTEXT};
enum CursorMode      { CURSOR_PASSTHROUGH, CURSOR_MOVE, CURSOR_RESIZE };

enum MessageType        { DEBUG, INFO, WARNING, ERROR, NMSG };
enum ClientType         { XDG_SHELL_CLIENT, LAYER_SHELL_CLIENT, XWL_MANAGED_CLIENT, XWL_UNMANAGED_CLIENT };
enum InputType          { INPUT_POINTER, INPUT_KEYBOARD, INPUT_MISC };
enum LayerType          { LyrBg, LyrBottom, LyrClient, LyrTop, LyrOverlay, LyrLock, NLayers }; // scene layers
enum NodeDescriptorType { NODE_CLIENT, NODE_XDG_POPUP, NODE_LAYER_SURFACE, NODE_LAYER_POPUP };
enum Direction          { LEFT, RIGHT, UP, DOWN };

//--- structs -----
struct simple_config {
   int n_tags;
   char tag_names[MAX_TAGS][32];
   int border_width;
   int tile_gap_width;
   bool sloppy_focus;
   int moveresize_step;

   float background_colour[4];
   float border_colour[NBORDERCOL][4];

   struct wl_list key_bindings;
   struct wl_list mouse_bindings;
   struct wl_list autostarts;
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

struct autostart {
   struct wl_list link;
   char command[128];
};

//--- global variables -----
extern struct simple_server* g_server;

//--- functions in config.c -----
struct simple_config * readConfiguration(char*);

//--- functions in main.c -----
void say(int, const char*, ...);
void spawn(char*);

#endif
