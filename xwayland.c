#include <wayland-server-core.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/xwayland.h>

#include "globals.h"
#include "server.h"
#include "seat.h"
#include "xwayland.h"

//------------------------------------------------------------------------
static void map_notify(struct wl_listener *listener, void *data) {
   say(INFO, "xwayland map_notify");
   struct simple_view *view = wl_container_of(listener, view, map);
   struct simple_server *server = view->server;
   view->mapped = true;

   view->x = server->seat->cursor->x;
   view->y = server->seat->cursor->y;
   view->w = view->xwayland_surface->width;
   view->h = view->xwayland_surface->height;
   view->x -= view->w/2;
   view->y -= view->h/2;

   view->surface = view->xwayland_surface->surface;

   focus_view(view);
}

static void request_configure_notify(struct wl_listener *listener, void *data) {
   say(INFO, "xwayland request_configure_notify");
   struct simple_view *view = wl_container_of(listener, view, request_configure);
   struct wlr_xwayland_surface_configure_event *event = data;
   wlr_xwayland_surface_configure(view->xwayland_surface, event->x, event->y, event->width, event->height);

}

//------------------------------------------------------------------------
void xwayland_unmanaged_create(struct simple_server *server, struct wlr_xwayland_surface *xsurface) {
   //
}

void new_xwayland_surface_notify(struct wl_listener *listener, void *data) {

   say(INFO, "new_xwayland_surface_notify");
   struct simple_server *server = wl_container_of(listener, server, new_xwayland_surface);
   struct wlr_xwayland_surface *xsurface = data;
   wlr_xwayland_surface_ping(xsurface);

   if(xsurface->override_redirect){
      xwayland_unmanaged_create(server, xsurface);
      return;
   }

   struct simple_view *view = calloc(1, sizeof(struct simple_view));
   view->server = server;
   view->xwayland_surface = xsurface;

   view->map.notify = map_notify;
   wl_signal_add(&xsurface->events.map, &view->map);
   //view->unmap.notify = unmap_notify;
   //wl_signal_add(&xsurface->events.unmap, &view->unmap);
   //view->destroy.notify = destroy_notify;
   //wl_signal_add(&xsurface->events.destroy, &view->destroy);
   view->request_configure.notify = request_configure_notify;
   wl_signal_add(&xsurface->events.request_configure, &view->request_configure);

   wl_list_insert(&view->server->views, &view->link);
}
