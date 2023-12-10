#include <wayland-server-core.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <xkbcommon/xkbcommon.h>

#include "globals.h"
#include "server.h"
#include "layer.h"
#include "seat.h"

void arrange_layers(struct simple_output *output) {
   say(INFO, "arrange_layers");
}

struct simple_output* output_from_wlr_output(struct simple_server *server, struct wlr_output*wlr_output){
   struct simple_output *output;
   wl_list_for_each(output, &server->outputs, link) {
      if(output->wlr_output == wlr_output) return output;
   }
   return NULL;
}

static void render_layer_surface(struct wlr_surface *surface, int sx, int sy, void *data) {
   say(INFO, "render_layer_surface");
}

//--- Notify functions ---------------------------------------------------
static void surface_commit_notify(struct wl_listener *listener, void *data) {
   //
   say(INFO, "surface_commit_notify");
}

static void output_destroy_notify(struct wl_listener *listener, void *data) {
   //
   say(INFO, "output_destroy_notify");
}

static void surface_destroy_notify(struct wl_listener *listener, void *data) {
   //
   say(INFO, "surface_destroy_notify");
}

static void surface_map_notify(struct wl_listener *listener, void *data) {
   //
   say(INFO, "surface_map_notify");
}

static void surface_unmap_notify(struct wl_listener *listener, void *data) {
   //
   say(INFO, "surface_unmap_notify");
}

static void new_layer_surface_notify(struct wl_listener *listener, void *data) {
   say(INFO, "new_layer_surface_notify");
   struct simple_server *server = wl_container_of(listener, server, new_layer_surface);
   struct wlr_layer_surface_v1 *layer_surface = data;

   if(!layer_surface->output) {
      struct wlr_output *output = wlr_output_layout_output_at(
            server->output_layout, server->seat->cursor->x, server->seat->cursor->y);
      layer_surface->output = output;
   }
   
   struct simple_output *output = output_from_wlr_output(server, layer_surface->output);

   struct simple_layer_surface *surface = calloc(1, sizeof(struct simple_layer_surface));
   surface->layer_surface = layer_surface;
   layer_surface->data = surface;
   surface->server = server;

   surface->surface_commit.notify = surface_commit_notify;
   wl_signal_add(&layer_surface->surface->events.commit, &surface->surface_commit);
   surface->output_destroy.notify = output_destroy_notify;
   wl_signal_add(&layer_surface->output->events.destroy, &surface->output_destroy);
   surface->destroy.notify = surface_destroy_notify;
   wl_signal_add(&layer_surface->events.destroy, &surface->destroy);
   surface->map.notify = surface_map_notify;
   wl_signal_add(&layer_surface->surface->events.map, &surface->map);
   surface->unmap.notify = surface_unmap_notify;
   wl_signal_add(&layer_surface->surface->events.unmap, &surface->unmap);

   wl_list_insert(&output->layers[layer_surface->pending.layer], &surface->link);

   struct wlr_layer_surface_v1_state old_state = layer_surface->current;
   layer_surface->current = layer_surface->pending;
   arrange_layers(output);
   layer_surface->current = old_state;
}

//------------------------------------------------------------------------
void initializeLayers(struct simple_server *server) {
   server->layer_shell = wlr_layer_shell_v1_create(server->display, 3);

   server->new_layer_surface.notify = new_layer_surface_notify;
   wl_signal_add(&server->layer_shell->events.new_surface, &server->new_layer_surface);
}

void render_layer(struct simple_output *output, struct wl_list *layer_surfaces) {
   struct simple_layer_surface *layer_surface;
   wl_list_for_each(layer_surface, layer_surfaces, link) {
      struct wlr_layer_surface_v1 *wlr_layer_surface_v1 = layer_surface->layer_surface;
      wlr_surface_for_each_surface(wlr_layer_surface_v1->surface, 
            render_layer_surface, layer_surface);
   }
}
