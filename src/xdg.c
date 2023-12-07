#include <assert.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>

#include "globals.h"
#include "server.h"
#include "seat.h"

//------------------------------------------------------------------------
static void xdg_map_notify(struct wl_listener *listener, void *data) {
   // called when the surface is mapped, or ready to display on-screen
   say(INFO, "xdg_map_notify");
   struct simple_view *view = wl_container_of(listener, view, map);

   wl_list_insert(&view->server->views, &view->link);
   view->mapped = true;
   
   focus_view(view, view->xdg_toplevel->base->surface);
}

static void xdg_unmap_notify(struct wl_listener *listener, void *data) {
   // called when the surface is unmapped, and should no longer be seen
   say(INFO, "xdg_unmap_notify");
   struct simple_view *view = wl_container_of(listener, view, unmap);
   // reset the cursor mode if the grabbed view was unmapped
   if(view == view->server->grabbed_view){
      view->server->cmode = CURSOR_PASSTHROUGH;
      view->server->grabbed_view = NULL;
   }
   view->mapped = false;

   wl_list_remove(&view->link);
}

static void xdg_destroy_notify(struct wl_listener *listener, void *data) {
   say(INFO, "xdg_destroy_notify");
   struct simple_view *view = wl_container_of(listener, view, destroy);
   
   wl_list_remove(&view->map.link);
   wl_list_remove(&view->unmap.link);
   wl_list_remove(&view->destroy.link);
   wl_list_remove(&view->request_move.link);
   wl_list_remove(&view->request_resize.link);
   //wl_list_remove(&view->link);

   free(view);
}

static void xdg_tl_request_move_notify(struct wl_listener *listener, void *data) {
   struct simple_view *view = wl_container_of(listener, view, request_move);
   begin_interactive(view, CURSOR_MOVE, 0);
}

static void xdg_tl_request_resize_notify(struct wl_listener *listener, void *data) {
   struct simple_view *view = wl_container_of(listener, view, request_resize);
   struct wlr_xdg_toplevel_resize_event *event = data;
   begin_interactive(view, CURSOR_RESIZE, event->edges);
}

//------------------------------------------------------------------------
void xdg_new_surface_notify(struct wl_listener *listener, void *data) {
   
   say(DEBUG, "new_xdg_surface_notify");
   struct simple_server *server = wl_container_of(listener, server, xdg_new_surface);
   struct wlr_xdg_surface *xdg_surface = data;

   // add xdg popups to the scene graph
   if(xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
      struct wlr_xdg_surface *parent = wlr_xdg_surface_from_wlr_surface(xdg_surface->popup->parent);
      struct wlr_scene_tree *parent_tree = parent->data;
      xdg_surface->data = wlr_scene_xdg_surface_create(parent_tree, xdg_surface);
      return;
   }
   assert(xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);

   // allocate a simple_view for this surface
   struct simple_view *view = calloc(1, sizeof(struct simple_view));
   view->server = server;
   view->type = XDG_SHELL_VIEW;
   view->xdg_surface = xdg_surface;
   view->xdg_toplevel = xdg_surface->toplevel;
   view->scene_tree = wlr_scene_xdg_surface_create(&view->server->scene->tree, view->xdg_toplevel->base);
   view->scene_tree->node.data = view;
   xdg_surface->data = view->scene_tree;

   view->map.notify = xdg_map_notify;
   wl_signal_add(&xdg_surface->events.map, &view->map);
   view->unmap.notify = xdg_unmap_notify;
   wl_signal_add(&xdg_surface->events.unmap, &view->unmap);
   view->destroy.notify = xdg_destroy_notify;
   wl_signal_add(&xdg_surface->events.destroy, &view->destroy);

   struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;
   view->request_move.notify = xdg_tl_request_move_notify;
   wl_signal_add(&toplevel->events.request_move, &view->request_move);
   view->request_resize.notify = xdg_tl_request_resize_notify;
   wl_signal_add(&toplevel->events.request_resize, &view->request_resize);
}

