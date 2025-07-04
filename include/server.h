#ifndef SERVER_H
#define SERVER_H

#include <wlr/types/wlr_scene.h>
#if XWAYLAND
#include <wlr/xwayland.h>
#endif

struct simple_server {
   struct wl_display *display;
   struct wl_event_loop *event_loop;

   struct wlr_backend *backend;
   struct wlr_renderer *renderer;
   struct wlr_allocator *allocator;
   struct wlr_compositor *compositor;

   struct wlr_scene *scene;
   struct wlr_scene_tree *layer_tree[NLayers];
   struct wlr_scene_output_layout *scene_output_layout;

   // tags
   unsigned int current_tag;
   unsigned int visible_tags;

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

   //struct wlr_foreign_toplevel_manager_v1 *foreign_toplevel_manager;
   //struct wlr_ext_foreign_toplevel_list_v1 *foreign_toplevel_list;
   
   struct wlr_xdg_activation_v1 *xdg_activation;
   struct wl_listener request_activate;

   // clients and layers
   struct wl_list clients;
   struct wlr_xdg_shell *xdg_shell;
   struct wl_listener xdg_new_toplevel;
   struct wl_listener xdg_new_popup;
#if XWAYLAND
   struct wlr_xwayland *xwayland;
   struct wl_listener xwl_new_surface;
   struct wl_listener xwl_ready;

   xcb_atom_t netatom[NetLast];
#endif

   struct wl_list layer_shells;
   struct wlr_layer_shell_v1 *layer_shell;
   struct wl_listener layer_new_surface;

   // --- seat and input ---
   struct wlr_seat *seat;

   struct wl_list inputs;
   struct wl_listener new_input;

   struct wlr_cursor *cursor;
   //bool cursor_hidden;
   struct wlr_xcursor_manager *cursor_manager;
   struct wlr_relative_pointer_manager_v1 *relative_pointer_manager;
   struct wlr_pointer_constraints_v1 *pointer_constraints;
   struct wlr_pointer_constraint_v1 *active_constraint;
   struct wl_listener new_pointer_constraint;

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

   // tablet
   struct wlr_tablet_manager_v2 *tablet_manager;
   struct wl_list tablet_tools;

   struct wl_listener tablet_tool_tip;
   struct wl_listener tablet_tool_proximity;
   struct wl_listener tablet_tool_axis;
   struct wl_listener tablet_tool_button;

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

   // background layer
   struct wlr_scene_rect *root_bg;

   struct simple_client *grabbed_client;
   struct simple_outline *grabbed_client_outline;
   double grab_x, grab_y;
   struct wlr_box grab_box;
   uint32_t resize_edges;
};

struct simple_outline {
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

struct simple_outline* simple_outline_create(struct wlr_scene_tree*, float*, int);
void simple_outline_set_size(struct simple_outline*, int, int);

void check_idle_inhibitor();

void print_server_info();

void setCurrentTag(int, bool);
void tileTag();

void prepareServer();
void startServer();
void cleanupServer();

#endif
