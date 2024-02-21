#ifndef GLOBALS_H
#define GLOBALS_H

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <wayland-server-core.h>
#include <wlr/backend/session.h>
#include <xkbcommon/xkbcommon.h>

#define XDG_SHELL_VERSION (6)
#define LAYER_SHELL_VERSION (4)
#define COMPOSITOR_VERSION (5)
#define FRAC_SCALE_VERSION (1)
#define DWL_IPC_VERSION (2)

#define N_LAYER_SHELL_LAYERS 4

// macros
#define LISTEN(E, L, H)    wl_signal_add((E), ((L)->notify = (H), (L)))
#define LENGTH(X)          (sizeof X / sizeof X[0])
#define TAGMASK(T)         (1 << (T))
#define VISIBLEON(C, O)    ((O) && (C)->output==(O) && ((C)->fixed || ((C)->tag & (O)->visible_tags)))
#define MIN(A, B)          ((A)<(B) ? (A) : (B))
#define MAX(A, B)          ((A)>(B) ? (A) : (B))

//--- enums -----
enum BorderColours   { FOCUSED, UNFOCUSED, URGENT, MARKED, FIXED, OUTLINE, NBORDERCOL };
enum KeyFunctions    { SPAWN, QUIT, LOCK, TAG, CLIENT, NFUNC };
enum MouseContext    { CONTEXT_ROOT, CONTEXT_CLIENT, NCONTEXT};
enum CursorMode      { CURSOR_NORMAL, CURSOR_MOVE, CURSOR_RESIZE, CURSOR_PRESSED };

enum MessageType        { DEBUG, INFO, WARNING, ERROR, NMSG };
enum ClientType         { XDG_SHELL_CLIENT, LAYER_SHELL_CLIENT, XWL_MANAGED_CLIENT, XWL_UNMANAGED_CLIENT };
enum InputType          { INPUT_POINTER, INPUT_KEYBOARD, INPUT_MISC };
enum LayerType          { LyrBg, LyrBottom, LyrClient, LyrTop, LyrOverlay, LyrLock, NLayers }; // scene layers
enum NodeDescriptorType { NODE_CLIENT, NODE_XDG_POPUP, NODE_LAYER_SURFACE, NODE_LAYER_POPUP };
enum Direction          { LEFT, RIGHT, UP, DOWN };
#ifdef XWAYLAND
enum NetAtoms  {NetWMWindowTypeDialog, NetWMWindowTypeSplash, NetWMWindowTypeToolbar, NetWMWindowTypeUtility, NetLast };
#endif

//--- structs -----
struct simple_config {
   char config_file_name[64];

   int n_tags;
   int border_width;
   int tile_gap_width;
   bool sloppy_focus;
   int moveresize_step;
   bool touchpad_tap_click;

   float background_colour[4];
   float border_colour[NBORDERCOL][4];

   char lock_cmd[64];
   char autostart_script[64];

   char xkb_layout[32];
   char xkb_options[32];

   struct wl_list key_bindings;
   struct wl_list mouse_bindings;
};

struct keymap {
   uint32_t mask;
   xkb_keysym_t keysym;
   int keyfn;
   char argument[64];
   
   struct wl_list link;
};

struct mousemap {
   uint32_t mask;
   uint32_t button;
   int context;
   char argument[64];
   
   struct wl_list link;
};

//--- global variables -----
extern struct simple_server* g_server;
extern struct wlr_session* g_session;
extern struct simple_config* g_config;

//--- functions in config.c -----
void readConfiguration(char*);
void reloadConfiguration();

//--- functions in main.c -----
void say(int, const char*, ...);
void spawn(char*);
void send_signal(int);

#endif
