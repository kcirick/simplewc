#include <wayland-server-core.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_scene.h>
#include <xkbcommon/xkbcommon.h>

#include "globals.h"
#include "server.h"
#include "layer.h"
#include "seat.h"

void arrange_layer(struct wl_list *list, const struct wlr_box *full_area, struct wlr_box *usable_area, bool exclusive) {
   struct simple_layer_surface *surface;
   wl_list_for_each(surface, list, link) {
      struct wlr_scene_layer_surface_v1 *scene = surface->scene_layer_surface;
      if(!!scene->layer_surface->current.exclusive_zone != exclusive) 
         continue;
      wlr_scene_layer_surface_v1_configure(scene, full_area, usable_area);
   }
}

void arrange_layers(struct simple_output *output) {
   say(INFO, "arrange_layers");

   struct wlr_box full_area = { 0 };
   wlr_output_effective_resolution(output->wlr_output, &full_area.width, &full_area.height);
   struct wlr_box usable_area = full_area;

   //apply_override(output, &usable_area);

   struct simple_server *server = output->server;
   struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(server->scene, output->wlr_output);
   if(!scene_output)
      say(ERROR, "no wlr_scene_output");

   for(int i=N_LAYER_SHELL_LAYERS-1; i>=0; i--){
      // process exclusive-zone clients from top to bottom
      arrange_layer(&output->layers[i], &full_area, &usable_area, /* exclusive */ true);
   }

   for(int i=0; i<N_LAYER_SHELL_LAYERS; i++){
      arrange_layer(&output->layers[i], &full_area, &usable_area, /* exclusive */ false);

      // set node position to account for output layout change
      wlr_scene_node_set_position(&output->layer_tree[i]->node, scene_output->x, scene_output->y);
   }
   output -> usable_area = usable_area;
}

//--- Notify functions ---------------------------------------------------
static void layer_surface_map_notify(struct wl_listener *listener, void *data) {
   //
   say(INFO, "layer_surface_map_notify");
}

static void layer_surface_unmap_notify(struct wl_listener *listener, void *data) {
   //
   say(INFO, "layer_surface_unmap_notify");
}

static void layer_surface_commit_notify(struct wl_listener *listener, void *data) {
   //
   say(INFO, "layer_surface_commit_notify");
}

static void layer_surface_destroy_notify(struct wl_listener *listener, void *data) {
   //
   say(INFO, "layer_surface_destroy_notify");
}

//------------------------------------------------------------------------
void layer_new_surface_notify(struct wl_listener *listener, void *data) {
   say(INFO, "layer_new_surface_notify");
   struct simple_server *server = wl_container_of(listener, server, layer_new_surface);
   struct wlr_layer_surface_v1 *layer_surface = data;

   if(!layer_surface->output) {
      layer_surface->output = wlr_output_layout_output_at(
            server->output_layout, server->seat->cursor->x, server->seat->cursor->y);
   }

   struct simple_output *output = layer_surface->output->data;

   struct wlr_scene_tree *selected_layer = output->layer_tree[layer_surface->current.layer];

   struct simple_layer_surface *lsurface = calloc(1, sizeof(struct simple_layer_surface));
   lsurface->scene_layer_surface = wlr_scene_layer_surface_v1_create(selected_layer, layer_surface);
   if(!lsurface->scene_layer_surface){
      wlr_layer_surface_v1_destroy(layer_surface);
      say(ERROR, "Could not create layer surface");
   }
   lsurface->server = server;
   lsurface->scene_layer_surface->layer_surface = layer_surface;

   lsurface->map.notify = layer_surface_map_notify;
   wl_signal_add(&layer_surface->surface->events.map, &lsurface->map);
   lsurface->unmap.notify = layer_surface_unmap_notify;
   wl_signal_add(&layer_surface->surface->events.unmap, &lsurface->unmap);
   lsurface->surface_commit.notify = layer_surface_commit_notify;
   wl_signal_add(&layer_surface->surface->events.commit, &lsurface->surface_commit);
   lsurface->destroy.notify = layer_surface_destroy_notify;
   wl_signal_add(&layer_surface->events.destroy, &lsurface->destroy);

   wl_list_insert(&output->layers[layer_surface->pending.layer], &lsurface->link);

   struct wlr_layer_surface_v1_state old_state = layer_surface->current;
   layer_surface->current = layer_surface->pending;
   arrange_layers(output);
   layer_surface->current = old_state;
}

