#ifndef SERVER_H
#define SERVER_H

enum cursor_mode { CURSOR_PASSTHROUGH, CURSOR_MOVE, CURSOR_RESIZE };

struct simple_server {
   struct wl_display *display;

   struct simple_config *config;

   struct wlr_backend *backend;
   struct wlr_renderer *renderer;
   struct wlr_output_layout *output_layout;

   struct wlr_compositor *compositor;

   struct wl_list views;
   struct wlr_xdg_shell *xdg_shell;
   struct wl_listener new_xdg_surface;
#if XWAYLAND
   struct wlr_xwayland *xwayland;
   struct wl_listener new_xwayland_surface;
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

   struct wl_list layers[4];
};

struct simple_view {
   struct wl_list link;
   struct simple_server *server;
   struct wlr_surface *surface;
   struct wlr_xdg_surface *xdg_surface;
#if XWAYLAND
   struct wlr_xwayland_surface *xwayland_surface;
#endif

   struct wl_listener map;
   struct wl_listener unmap;
   struct wl_listener destroy;
   struct wl_listener commit;
   struct wl_listener request_move;
   struct wl_listener request_resize;
   struct wl_listener request_configure;

   bool mapped;
   int x, y, w, h;
};

void prepareServer(struct simple_server*, struct simple_config*);
void runServer(struct simple_server*);
void cleanupServer(struct simple_server*);
struct simple_view* desktop_view_at(struct simple_server*, double, double, struct wlr_surface**, double*, double*);
void focus_view(struct simple_view*);
void begin_interactive(struct simple_view*, enum cursor_mode, uint32_t);

#endif
