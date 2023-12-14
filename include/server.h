#ifndef SERVER_H
#define SERVER_H

struct simple_server {
   struct wl_display *display;

   struct simple_config *config;

   struct wlr_backend *backend;
   struct wlr_renderer *renderer;
   struct wlr_allocator *allocator;
   struct wlr_compositor *compositor;

   struct wlr_scene *scene;
   struct wlr_scene_tree *layer_tree[4];
   struct wlr_scene_output_layout *scene_output_layout;

   struct wl_list outputs;
   struct wlr_output_layout *output_layout;
   struct wl_listener new_output;
   struct wl_listener output_layout_change;

   struct wlr_output_manager_v1 *output_manager;
   struct wl_listener output_manager_apply;
   struct wl_listener output_manager_test;

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

   //struct simple_seat *seat;
   struct wlr_seat *seat;

   struct wl_list inputs;   
   struct wl_listener new_input;

   struct wlr_cursor *cursor;
   struct wlr_xcursor_manager *cursor_manager;
   enum cursor_mode cursor_mode;
   struct wl_listener cursor_motion;
   struct wl_listener cursor_motion_abs;
   struct wl_listener cursor_button;
   struct wl_listener cursor_axis;
   struct wl_listener cursor_frame;

   struct wl_listener request_cursor;
   struct wl_listener request_set_selection;

   struct simple_client *grabbed_client;
   double grab_x, grab_y;
   struct wlr_box grab_box;
   uint32_t resize_edges;
};

struct simple_output {
   struct wl_list link;
   struct simple_server *server;
   struct wlr_output *wlr_output;

   struct wl_list layers[4];

   struct wl_listener frame;
   struct wl_listener request_state;
   struct wl_listener destroy;

   struct wlr_box usable_area;
};

struct simple_input {
   struct wl_list link;
   struct simple_server *server;
   struct wlr_input_device *device;
   struct wlr_keyboard *keyboard;

   enum inputType type;
   struct wl_listener kb_modifiers;
   struct wl_listener kb_key;
   struct wl_listener destroy;
};

void input_focus_surface(struct simple_server*, struct wlr_surface*);

void prepareServer(struct simple_server*, struct wlr_session*, int);
void startServer(struct simple_server*);
void cleanupServer(struct simple_server*);

void quitServer(struct simple_server*);

#endif
