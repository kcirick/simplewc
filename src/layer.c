#include <wayland-server-core.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_scene.h>
#include <xkbcommon/xkbcommon.h>

#include "globals.h"
#include "server.h"
#include "layer.h"

void 
arrange_layer(struct wl_list *list, const struct wlr_box *full_area, struct wlr_box *usable_area, bool exclusive) 
{
   struct simple_layer_surface *surface;
   wl_list_for_each(surface, list, link) {
      struct wlr_scene_layer_surface_v1 *scene = surface->scene_layer_surface;
      if(!!scene->layer_surface->current.exclusive_zone != exclusive) 
         continue;
      wlr_scene_layer_surface_v1_configure(scene, full_area, usable_area);
   }
}

void
arrange_layers(struct simple_output *output)
{
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
      wlr_scene_node_set_position(&server->layer_tree[i]->node, scene_output->x, scene_output->y);
   }
   output -> usable_area = usable_area;
}

//--- Notify functions ---------------------------------------------------
static void 
layer_surface_map_notify(struct wl_listener *listener, void *data) 
{
   //
   say(INFO, "layer_surface_map_notify");
}

static void 
layer_surface_unmap_notify(struct wl_listener *listener, void *data) 
{
   //
   say(INFO, "layer_surface_unmap_notify");
}

static void 
layer_surface_commit_notify(struct wl_listener *listener, void *data) 
{
   //
   say(INFO, "layer_surface_commit_notify");
}

static void 
layer_surface_destroy_notify(struct wl_listener *listener, void *data) 
{
   //
   say(INFO, "layer_surface_destroy_notify");
}

//------------------------------------------------------------------------
void 
layer_new_surface_notify(struct wl_listener *listener, void *data) 
{
   say(INFO, "layer_new_surface_notify");
   struct simple_server *server = wl_container_of(listener, server, layer_new_surface);
   struct wlr_layer_surface_v1 *layer_surface = data;

   if(!layer_surface->output) {
      layer_surface->output = wlr_output_layout_output_at(
            server->output_layout, server->cursor->x, server->cursor->y);
   }

   struct simple_output *output = layer_surface->output->data;

   struct wlr_scene_tree *selected_layer = server->layer_tree[layer_surface->pending.layer];

   struct simple_layer_surface *lsurface = calloc(1, sizeof(struct simple_layer_surface));
   lsurface->scene_layer_surface = wlr_scene_layer_surface_v1_create(selected_layer, layer_surface);
   if(!lsurface->scene_layer_surface){
      wlr_layer_surface_v1_destroy(layer_surface);
      say(ERROR, "Could not create layer surface");
   }
   lsurface->type = LAYER_SHELL_CLIENT;
   lsurface->server = server;
   lsurface->output = output;
   lsurface->scene_layer_surface->layer_surface = layer_surface;
   lsurface->scene_tree = lsurface->scene_layer_surface->tree;
   lsurface->popups = wlr_scene_tree_create(selected_layer);
   layer_surface->data = lsurface;
   lsurface->scene_tree->node.data = lsurface;

   LISTEN(&layer_surface->surface->events.map, &lsurface->map, layer_surface_map_notify);
   LISTEN(&layer_surface->surface->events.unmap, &lsurface->unmap, layer_surface_unmap_notify);
   LISTEN(&layer_surface->surface->events.commit, &lsurface->surface_commit, layer_surface_commit_notify);
   LISTEN(&layer_surface->events.destroy, &lsurface->destroy, layer_surface_destroy_notify);

   wl_list_insert(&output->layers[layer_surface->pending.layer], &lsurface->link);

   struct wlr_layer_surface_v1_state old_state = layer_surface->current;
   layer_surface->current = layer_surface->pending;
   arrange_layers(output);
   layer_surface->current = old_state;
}

