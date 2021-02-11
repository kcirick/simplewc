#include <wayland-server-core.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xdg_shell.h>

#include "globals.h"
#include "server.h"
#include "seat.h"

//------------------------------------------------------------------------
static void map_notify(struct wl_listener *listener, void *data) {
   say(INFO, "map_notify");
   struct simple_view *view = wl_container_of(listener, view, map);
   struct simple_server *server = view->server;
   view->mapped = true;

   struct wlr_output *output = wlr_output_layout_output_at(server->output_layout, server->seat->cursor->x, server->seat->cursor->y);
   struct wlr_output_layout_output *layout = wlr_output_layout_get(server->output_layout, output);
   if(view->x == -1 || view->y == -1) {
      struct wlr_surface_state *current = &view->xdg_surface->surface->current;
      int owidth, oheight;
      wlr_output_effective_resolution(output, &owidth, &oheight);
      //move_view(view, layout->x + (owidth/2 - current->width/2),
      //                layout->y + (oheight/2 - current->height/2));
   } //else
      //move_view(view, view->x, view->y);

   focus_view(view);
}

static void unmap_notify(struct wl_listener *listener, void *data) {
   say(INFO, "unmap_notify");
   struct simple_view *view = wl_container_of(listener, view, unmap);
   view->mapped = false;
}

static void destroy_notify(struct wl_listener *listener, void *data) {
   say(INFO, "destroy_notify");
   struct simple_view *view = wl_container_of(listener, view, destroy);
   wl_list_remove(&view->link);
   free(view);
}

static void request_move_notify(struct wl_listener *listener, void *data) {
   struct simple_view *view = wl_container_of(listener, view, request_move);
   begin_interactive(view, CURSOR_MOVE, 0);
}

static void request_resize_notify(struct wl_listener *listener, void *data) {
   struct simple_view *view = wl_container_of(listener, view, request_resize);
   struct wlr_xdg_toplevel_resize_event *event = data;
   begin_interactive(view, CURSOR_RESIZE, event->edges);
}

//------------------------------------------------------------------------
void new_xdg_surface_notify(struct wl_listener *listener, void *data) {
   
   say(INFO, "new_xdg_surface_notify");
   struct simple_server *server = wl_container_of(listener, server, new_xdg_surface);
   struct wlr_xdg_surface *xdg_surface = data;
   if(xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) return;

   struct simple_view *view = calloc(1, sizeof(struct simple_view));
   view->server = server;
   view->xdg_surface = xdg_surface;

   view->map.notify = map_notify;
   wl_signal_add(&xdg_surface->events.map, &view->map);
   view->unmap.notify = unmap_notify;
   wl_signal_add(&xdg_surface->events.unmap, &view->unmap);
   view->destroy.notify = destroy_notify;
   wl_signal_add(&xdg_surface->events.destroy, &view->destroy);

   struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;
   view->request_move.notify = request_move_notify;
   wl_signal_add(&toplevel->events.request_move, &view->request_move);
   view->request_resize.notify = request_resize_notify;
   wl_signal_add(&toplevel->events.request_resize, &view->request_resize);
   
   wl_list_insert(&server->views, &view->link);
}

