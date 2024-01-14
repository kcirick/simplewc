#ifndef SERVER_H
#define SERVER_H

#include <wlr/types/wlr_scene.h>
#if XWAYLAND
#include <wlr/xwayland.h>
#endif

struct simple_server {
   struct wl_display *display;

   struct wlr_backend *backend;
   struct wlr_renderer *renderer;
   struct wlr_allocator *allocator;
   struct wlr_compositor *compositor;

   struct wlr_scene *scene;
   struct wlr_scene_tree *layer_tree[NLayers];
   struct wlr_scene_output_layout *scene_output_layout;

   // output and decoration manager
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

   // clients and layers
   struct wl_list clients;
   struct wlr_xdg_shell *xdg_shell;
   struct wl_listener xdg_new_surface;
#if XWAYLAND
   struct wlr_xwayland *xwayland;
   struct wl_listener xwl_new_surface;
   struct wl_listener xwl_ready;

   xcb_atom_t netatom[NetLast];
#endif

   struct wl_list layer_shells;
   struct wlr_layer_shell_v1 *layer_shell;
   struct wl_listener layer_new_surface;

   // seat and input
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

   struct wlr_scene_tree *drag_icon;
   struct wl_listener request_start_drag;
   struct wl_listener start_drag;
   struct wl_listener destroy_drag_icon;

   // input method
   struct simple_input_method_relay *im_relay;
   struct wlr_text_input_manager_v3 *text_input;
   struct wlr_input_method_manager_v2 *input_method;

   // session idle notifier
   struct wlr_idle_notifier_v1 *idle_notifier;
   struct wlr_idle_inhibit_manager_v1 *idle_inhibit_manager;
   struct wl_listener new_inhibitor;
   struct wl_listener inhibitor_destroy;

   // session lock
   struct wlr_session_lock_manager_v1 *session_lock_manager;
   struct wlr_scene_rect *locked_bg;
   struct wlr_session_lock_v1 *cur_lock;
   struct wl_listener new_lock_session_manager;
   struct wl_listener lock_session_manager_destroy;
   bool locked;

   // session power manager
   struct wlr_output_power_manager_v1 *output_power_manager;
   struct wl_listener output_pm_set_mode;

   // gamma control manager
   struct wlr_gamma_control_manager_v1 *gamma_control_manager;
   struct wl_listener set_gamma;

   // background layer
   struct wlr_scene_rect *root_bg;

   struct simple_client *grabbed_client;
   struct client_outline *grabbed_client_outline;
   double grab_x, grab_y;
   struct wlr_box grab_box;
   uint32_t resize_edges;
};

struct simple_output {
   struct wl_list link;
   struct wlr_output *wlr_output;

   struct wl_list layer_shells[N_LAYER_SHELL_LAYERS];

   struct wl_list ipc_outputs; // ipc addition

   struct wl_listener frame;
   struct wl_listener request_state;
   struct wl_listener destroy;

   unsigned int current_tag;
   unsigned int visible_tags;

   struct wlr_session_lock_surface_v1 *lock_surface;
   struct wl_listener lock_surface_destroy;

   struct wlr_box full_area;
   struct wlr_box usable_area;

   bool gamma_lut_changed;
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

struct simple_session_lock {
   struct wlr_scene_tree *scene;

   struct wlr_session_lock_v1 *lock;
   struct wl_listener new_surface;
   struct wl_listener unlock;
   struct wl_listener destroy;
};

struct client_outline* client_outline_create(struct wlr_scene_tree*, float*, int);
void client_outline_set_size(struct client_outline*, int, int);

void print_server_info();
void setCurrentTag(int, bool);
void tileTag();
void arrange_output(struct simple_output*);

void prepareServer();
void startServer(char*);
void cleanupServer();

#endif
