#include <assert.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/xwayland.h>
#include <wlr/util/box.h>
#include <xkbcommon/xkbcommon.h>

#include "globals.h"
#include "server.h"
#include "seat.h"
#include "xwayland.h"

struct wlr_xwayland_surface * xwayland_surface_from_view(struct simple_view* view){
   assert(view->type == XWAYLAND_VIEW);
   assert(view->xwayland_surface);
   return view->xwayland_surface;
}

static void set_initial_geometry(struct simple_view* view) {
   if(wlr_box_empty(&view->current)){
      struct wlr_xwayland_surface* xwl_surface = xwayland_surface_from_view(view);
      view->current.x = xwl_surface->x;
      view->current.y = xwl_surface->y;
      view->current.width = xwl_surface->width;
      view->current.height = xwl_surface->height;
   }
}

//------------------------------------------------------------------------
static void xwl_surface_destroy_notify(struct wl_listener *listener, void *data) {
   //
}

static void xwl_commit_notify(struct wl_listener *listener, void *data) {
   say(INFO, "xwl_commit_notify");
   struct simple_view *view = wl_container_of(listener, view, commit);
   //assert(data && data==view->surface);

   //struct wlr_surface_state *state = &view->surface->current;
   //struct wlr_box *current = &view->current;

   //if(current->width != state->width || current->height != state->height)
   //   view_apply_geometry(view, state->width, state->height);
}

static void xwl_map_notify(struct wl_listener *listener, void *data) {
   say(INFO, "xwl_map_notify");
   struct simple_view *view = wl_container_of(listener, view, map);
   //struct simple_server *server = view->server;

   struct wlr_xwayland_surface *xwl_surface = xwayland_surface_from_view(view);
   if(view->mapped) return;
   if(!xwl_surface->surface){
      say(DEBUG, "Cannot map view without wlr_surface");
      return;
   }

   view->mapped = true;

   wlr_scene_node_set_enabled(&view->scene_tree->node, true);
   // full screen check goes here
   //if(!view->fullscreen && xwayland_surface->fullscreen) ...

   if(view->surface != xwl_surface->surface) {
      if(view->surface)
         wl_list_remove(&view->surface_destroy.link);
      view->surface = xwl_surface->surface;

      view->surface_destroy.notify = xwl_surface_destroy_notify;
      wl_signal_add(&view->surface->events.destroy, &view->surface_destroy);

      // will be freed automatically once the surface is being destroy
      struct wlr_scene_tree *tree = wlr_scene_subsurface_tree_create(view->scene_tree, view->surface);
      if(!tree){
         wl_resource_post_no_memory(view->surface->resource);
         return;
      }
      view->scene_node = &tree->node;
   }

   say (DEBUG, ">>> HERE 2");
   //if(!view->toplevel.handle) init_foreign_toplevel(view);

   //if(!view->been_mapped) ...
   set_initial_geometry(view);

   view->commit.notify = xwl_commit_notify;
   wl_signal_add(&xwl_surface->surface->events.commit, &view->commit);

   //view_impl_map():
   //desktop_focus_and_activate_view();
   //view_move_to_front(view);
   //..

   wl_list_insert(&view->server->views, &view->link);

   focus_view(view, view->xwayland_surface->surface);
   /*
   view->x = server->seat->cursor->x;
   view->y = server->seat->cursor->y;
   view->w = view->xwayland_surface->width;
   view->h = view->xwayland_surface->height;
   view->x -= view->w/2;
   view->y -= view->h/2;

   view->surface = view->xwayland_surface->surface;

   wl_list_insert(&view->server->views, &view->link);
   */   
}

static void xwl_unmap_notify(struct wl_listener *listener, void *data) {
   say(INFO, "xwl_unmap_notify");
   struct simple_view *view = wl_container_of(listener, view, unmap);

   view->mapped=false;

   wl_list_remove(&view->link);
}

static void xwl_destroy_notify(struct wl_listener *listener, void*data) {
   say(INFO, "xwl_destroy_notify");
   struct simple_view *view = wl_container_of(listener, view, destroy);

   wl_list_remove(&view->map.link);
   wl_list_remove(&view->unmap.link);
   wl_list_remove(&view->destroy.link);
   wl_list_remove(&view->request_configure.link);

   free(view);
}

static void xwl_request_configure_notify(struct wl_listener *listener, void *data) {
   say(INFO, "xwl_request_configure_notify");
   struct simple_view *view = wl_container_of(listener, view, request_configure);
   struct wlr_xwayland_surface_configure_event *event = data;
   wlr_xwayland_surface_configure(view->xwayland_surface, event->x, event->y, event->width, event->height);
}


//------------------------------------------------------------------------
void xwayland_unmanaged_create(struct simple_server *server, struct wlr_xwayland_surface *xsurface) {
   //
   say(DEBUG, "xwayland_unmanaged_create");
}

void xwayland_ready_notify(struct wl_listener *listener, void *data) {
   say(DEBUG, "xwayland_ready_notify");
   struct simple_server *server = wl_container_of(listener, server, xwayland_ready);
   wlr_xwayland_set_seat(server->xwayland, server->seat->seat);
}

void xwayland_new_surface_notify(struct wl_listener *listener, void *data) {

   say(INFO, "xwayland_new_surface_notify");
   struct simple_server *server = wl_container_of(listener, server, xwayland_new_surface);
   struct wlr_xwayland_surface *xsurface = data;
   wlr_xwayland_surface_ping(xsurface);

   if(xsurface->override_redirect){
      xwayland_unmanaged_create(server, xsurface);
      return;
   }

   // Create simple_view
   struct simple_view *xwl_view = calloc(1, sizeof(struct simple_view));

   xwl_view->server = server;
   xwl_view->type = XWAYLAND_VIEW;
   xwl_view->xwayland_surface = xsurface;
   xsurface->data = xwl_view;

   xwl_view->scene_tree = wlr_scene_tree_create(&xwl_view->server->scene->tree);
   xwl_view->scene_tree->node.data = xwl_view;

   xwl_view->map.notify = xwl_map_notify;
   wl_signal_add(&xsurface->events.map, &xwl_view->map);
   xwl_view->unmap.notify = xwl_unmap_notify;
   wl_signal_add(&xsurface->events.unmap, &xwl_view->unmap);
   xwl_view->destroy.notify = xwl_destroy_notify;
   wl_signal_add(&xsurface->events.destroy, &xwl_view->destroy);
   xwl_view->request_configure.notify = xwl_request_configure_notify;
   wl_signal_add(&xsurface->events.request_configure, &xwl_view->request_configure);
   // More mappings
   
   //wl_list_insert(&view->server->views, &view->link);
}

void initializeXWayland(struct simple_server *server) {

   say(DEBUG, "initializeXWayland");

   server->xwayland = wlr_xwayland_create(server->display, server->compositor, true);
   if(!server->xwayland)
      say(ERROR, "cannot create xwayland server");

   server->xwayland_new_surface.notify = xwayland_new_surface_notify;
   wl_signal_add(&server->xwayland->events.new_surface, &server->xwayland_new_surface);

   server->xwayland_ready.notify = xwayland_ready_notify;
   wl_signal_add(&server->xwayland->events.ready, &server->xwayland_ready);

}

void startXWayland(struct simple_server *server) {

   if(setenv("DISPLAY", server->xwayland->display_name, true) < 0)
      say(WARNING, "Unable to set DISPLAY for xwayland");
   else {
      wlr_log(WLR_DEBUG, "XWayland is running on display %s", server->xwayland->display_name);
      say(INFO, "XWayland is running on display %s", server->xwayland->display_name);
   }
   
   struct wlr_xcursor *xcursor;
   xcursor = wlr_xcursor_manager_get_xcursor(server->seat->cursor_manager, "left_ptr", 1);
   if(xcursor){
      struct wlr_xcursor_image *image = xcursor->images[0];
      wlr_xwayland_set_cursor(server->xwayland, image->buffer, 
            image->width*4, image->width, image->height, image->hotspot_x, image->hotspot_y);
   }

}

