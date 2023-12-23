#ifndef SERVER_H
#define SERVER_H

#include <wlr/types/wlr_scene.h>

struct simple_server {
   struct wl_display *display;

   struct simple_config *config;

   struct wlr_backend *backend;
   struct wlr_renderer *renderer;
   struct wlr_allocator *allocator;
   struct wlr_compositor *compositor;

   struct wlr_scene *scene;
   struct wlr_scene_tree *layer_tree[NLayers];
   struct wlr_scene_output_layout *scene_output_layout;

   struct wl_list outputs;
   struct simple_output* cur_output;
   struct wlr_output_layout *output_layout;
   struct wl_listener new_output;
   struct wl_listener output_layout_change;

   struct wlr_output_manager_v1 *output_manager;
   struct wl_listener output_manager_apply;
   struct wl_listener output_manager_test;

   struct wlr_xdg_decoration_manager_v1 *xdg_decoration_manager;
   struct wl_listener new_decoration;

   struct wl_list clients;
   struct wlr_xdg_shell *xdg_shell;
   struct wl_listener xdg_new_surface;
#if XWAYLAND
   struct wlr_xwayland *xwayland;
   struct wl_listener xwl_new_surface;
   struct wl_listener xwl_ready;
#endif

   struct wl_list layer_shells;
   struct wlr_layer_shell_v1 *layer_shell;
   struct wl_listener layer_new_surface;

   struct wl_list focus_order;

   struct wlr_seat *seat;

   struct wl_list inputs;   
   struct wl_listener new_input;

   struct wlr_cursor *cursor;
   struct wlr_xcursor_manager *cursor_manager;
   enum CursorMode cursor_mode;
   struct wl_listener cursor_motion;
   struct wl_listener cursor_motion_abs;
   struct wl_listener cursor_button;
   struct wl_listener cursor_axis;
   struct wl_listener cursor_frame;

   struct wl_listener request_cursor;
   struct wl_listener request_set_selection;
   struct wl_listener request_set_primary_selection;

   struct wlr_idle_notifier_v1 *idle_notifier;
   struct wlr_idle_inhibit_manager_v1 *idle_inhibit_manager;
   struct wl_listener new_inhibitor;
   struct wl_listener inhibitor_destroy;

   struct wlr_session_lock_manager_v1 *session_lock_manager;
   struct wlr_scene_rect *locked_bg;
   struct wlr_session_lock_v1 *cur_lock;
   struct wl_listener new_lock_session_manager;
   struct wl_listener lock_session_manager_destroy;

   struct wlr_scene_rect *root_bg;

   struct simple_client *grabbed_client;
   struct client_outline *grabbed_client_outline;
   double grab_x, grab_y;
   struct wlr_box grab_box;
   uint32_t resize_edges;
};

struct simple_output {
   struct wl_list link;
   struct simple_server *server;
   struct wlr_output *wlr_output;

   struct wl_list layer_shells[N_LAYER_SHELL_LAYERS];

   struct wl_list ipc_outputs; // ipc addition

   struct wl_listener frame;
   struct wl_listener request_state;
   struct wl_listener destroy;

   unsigned int current_tag;
   unsigned int visible_tags;

   struct wlr_box usable_area;
};

struct simple_input {
   struct wl_list link;
   struct simple_server *server;
   struct wlr_input_device *device;
   struct wlr_keyboard *keyboard;

   enum InputType type;
   struct wl_listener kb_modifiers;
   struct wl_listener kb_key;
   struct wl_listener destroy;
};

struct client_outline {
   struct wlr_scene_tree *tree;
   int line_width;

   struct wlr_scene_rect *top;
   struct wlr_scene_rect *bottom;
   struct wlr_scene_rect *left;
   struct wlr_scene_rect *right;

   struct wl_listener destroy;
};

struct client_outline* client_outline_create(struct wlr_scene_tree*, float*, int);
void client_outline_set_size(struct client_outline*, int, int);

void print_server_info(struct simple_server*);
void setCurrentTag(struct simple_server*, int, bool);
void tileTag(struct simple_server*);
void arrange_output(struct simple_output*);

void input_focus_surface(struct simple_server*, struct wlr_surface*);

void prepareServer(struct simple_server*, struct wlr_session*, int);
void startServer(struct simple_server*);
void cleanupServer(struct simple_server*);

#endif
