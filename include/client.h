#ifndef CLIENT_H
#define CLIENT_H

struct simple_client {
   struct wl_list link;
   struct simple_server *server;
   struct simple_output *output;
   enum client_type type;

   //struct wlr_surface *surface;
   struct wlr_xdg_surface *xdg_surface;
#if XWAYLAND
   struct wlr_xwayland_surface *xwayland_surface;
#endif
   //struct wlr_xdg_toplevel *xdg_toplevel;
   struct wlr_scene_tree *scene_tree;
   struct wlr_scene_node *scene_node;
   struct wlr_scene_rect *border[4]; // top, bottom, left, right

   struct wl_listener map;
   struct wl_listener unmap;
   struct wl_listener destroy;
   struct wl_listener commit;
   struct wl_listener request_move;
   struct wl_listener request_resize;
//   struct wl_listener request_configure;

#if XWAYLAND
   struct wl_listener associate;
   struct wl_listener dissociate;
   struct wl_listener surface_destroy;
   struct wl_listener request_activate;
   struct wl_listener request_configure;
   struct wl_listener set_app_id;
   struct wl_listener override_redirect;
#endif

   bool mapped;

   // geometry of the wlr_surface within the view as currently displayed
   struct wlr_box geom;
};

struct simple_client* get_client_at(struct simple_server*, double, double, struct wlr_surface**, double*, double*);
void focus_client(struct simple_client*, struct wlr_surface *surface);
void begin_interactive(struct simple_client*, enum cursor_mode, uint32_t);

void get_client_size(struct simple_client*, struct wlr_box*);
void set_client_size(struct simple_client*, struct wlr_box);
void set_client_border_colour(struct simple_client*, int);

void xdg_new_surface_notify(struct wl_listener*, void*);

void xwl_new_surface_notify(struct wl_listener*, void *);
void xwl_ready_notify(struct wl_listener*, void *);

#endif
