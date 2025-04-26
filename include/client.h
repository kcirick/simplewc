#ifndef CLIENT_H
#define CLIENT_H

#include "layer.h"

struct simple_client {
   struct wl_list link;
   struct simple_output *output;
   enum ClientType type;

   struct wlr_xdg_surface *xdg_surface;
#if XWAYLAND
   struct wlr_xwayland_surface *xwl_surface;
#endif
   struct wlr_scene_tree *scene_tree;
   struct wlr_scene_tree *scene_surface_tree;
   struct wlr_scene_rect *border[4]; // top, bottom, left, right

   struct wlr_xdg_toplevel_decoration_v1 *decoration;

   struct wl_listener map;
   struct wl_listener unmap;
   struct wl_listener destroy;
   struct wl_listener commit;
   struct wl_listener request_fullscreen;
//   struct wl_listener decoration_mode;
//   struct wl_listener decoration_destroy;

#if XWAYLAND
   struct wl_listener associate;
   struct wl_listener dissociate;
   struct wl_listener surface_destroy;
   struct wl_listener request_activate;
   struct wl_listener request_configure;
   struct wl_listener set_hints;
   struct wl_listener set_title;
#endif

   uint32_t tag;
   bool fixed;
   bool urgent;
   bool fullscreen;
   bool maximized;

   bool visible;

   // geometry of the wlr_surface within the view as currently displayed
   struct wlr_box geom;
   struct wlr_box prev_geom;

   bool resize_requested;
};
   
//--- action calls
void sendClientToTag(struct simple_client*, int);
void toggleClientFixed(struct simple_client*);
void toggleClientVisible(struct simple_client*);
void toggleClientFullscreen(struct simple_client*);
void toggleClientMaximize(struct simple_client*);
//void setClientFullscreen(struct simple_client*, int);
void cycleClients(struct simple_output*);
void killClient(struct simple_client*);
void tileClient(struct simple_client*, enum Direction);
//void maximizeClient(struct simple_client*);

char * get_client_title(struct simple_client*);
char * get_client_appid(struct simple_client*);
struct simple_client* get_top_client_from_output(struct simple_output*, bool);
int get_client_at(double, double, struct simple_client**, struct wlr_surface**, double*, double*);
int get_client_from_surface(struct wlr_surface*, struct simple_client**, struct simple_layer_surface**);
void focus_client(struct simple_client*, bool);
void begin_interactive(struct simple_client*, enum CursorMode, uint32_t);

void get_client_geometry(struct simple_client*, struct wlr_box*);
void set_client_geometry(struct simple_client*);
void set_client_border_colour(struct simple_client*, int);
void update_border_geometry(struct simple_client*);

void xdg_new_toplevel_notify(struct wl_listener*, void*);
void xdg_new_popup_notify(struct wl_listener*, void*);

void xwl_new_surface_notify(struct wl_listener*, void *);
void xwl_ready_notify(struct wl_listener*, void *);

#endif
