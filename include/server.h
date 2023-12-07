#ifndef SERVER_H
#define SERVER_H

enum cursor_mode { CURSOR_PASSTHROUGH, CURSOR_MOVE, CURSOR_RESIZE };

enum view_type {
   XDG_SHELL_VIEW,
#if XWAYLAND
   XWAYLAND_VIEW,
#endif
};

struct simple_server {
   struct wl_display *display;

   struct simple_config *config;

   struct wlr_backend *backend;
   struct wlr_renderer *renderer;
   struct wlr_allocator *allocator;
   struct wlr_compositor *compositor;
   struct wlr_output_layout *output_layout;
   struct wlr_scene *scene;


   struct wl_list views;
   struct wlr_xdg_shell *xdg_shell;
   struct wl_listener xdg_new_surface;
#if XWAYLAND
   struct wlr_xwayland *xwayland;
   struct wl_listener xwayland_new_surface;
   struct wl_listener xwayland_ready;
#endif

   struct wlr_layer_shell_v1 *layer_shell;
   struct wl_listener new_layer_surface;

   struct wl_list outputs;
   struct wl_listener new_output;

   struct simple_seat *seat;

   struct simple_view *grabbed_view;
   enum cursor_mode cmode;
   double grab_x, grab_y;
   struct wlr_box grab_box;
   uint32_t resize_edges;
};

struct simple_output {
   struct wl_list link;
   struct simple_server *server;
   struct wlr_output *wlr_output;
   struct wl_listener frame;
   struct wl_listener destroy;

   struct wl_list layers[4];
};

struct simple_view {
   struct wl_list link;
   struct simple_server *server;
   enum view_type type;

   struct wlr_surface *surface;
   struct wlr_xdg_surface *xdg_surface;
   struct wlr_xdg_toplevel *xdg_toplevel;
   struct wlr_scene_tree *scene_tree;
   struct wlr_scene_node *scene_node;
#if XWAYLAND
   struct wlr_xwayland_surface *xwayland_surface;
#endif

   // geometry of the wlr_surface within the view as currently displayed
   struct wlr_box current;

   struct wl_listener map;
   struct wl_listener unmap;
   struct wl_listener destroy;
   struct wl_listener commit;
   struct wl_listener request_move;
   struct wl_listener request_resize;
//   struct wl_listener request_configure;

#if XWAYLAND
   struct wl_listener surface_destroy;
   struct wl_listener request_activate;
   struct wl_listener request_configure;
   struct wl_listener set_app_id;
   struct wl_listener override_redirect;
#endif

   bool mapped;
   //int x, y, w, h;
};

void prepareServer(struct simple_server*, struct simple_config*);
void startServer(struct simple_server*);
void cleanupServer(struct simple_server*);
struct simple_view* desktop_view_at(struct simple_server*, double, double, struct wlr_surface**, double*, double*);
void focus_view(struct simple_view*, struct wlr_surface *surface);
void begin_interactive(struct simple_view*, enum cursor_mode, uint32_t);

#endif
