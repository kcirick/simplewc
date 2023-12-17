#include <wayland-server-core.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <xkbcommon/xkbcommon.h>

#include "globals.h"
#include "layer.h"
#include "client.h"
#include "server.h"

//map from ZWLR_LAYER_SHELL_* constants to Lyr* enum
static const int layermap[] = {LyrBg, LyrBottom, LyrTop, LyrOverlay };

void 
arrange_layer(struct wl_list *list, const struct wlr_box *full_area, struct wlr_box *usable_area, bool exclusive) 
{
   struct simple_layer_surface *surface;

   wl_list_for_each(surface, list, link) {
      struct wlr_layer_surface_v1 *wlr_lsurface = surface->scene_layer_surface->layer_surface;
      struct wlr_layer_surface_v1_state *state = &wlr_lsurface->current;
      
      if(exclusive != (state->exclusive_zone>0)) 
         continue;
      
      wlr_scene_layer_surface_v1_configure(surface->scene_layer_surface, full_area, usable_area);
      wlr_scene_node_set_position(&surface->popups->node, surface->scene_tree->node.x, surface->scene_tree->node.y);
      surface->geom.x = surface->scene_tree->node.x;
      surface->geom.y = surface->scene_tree->node.y;
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
   if(!scene_output){
      say(DEBUG, "no wlr_scene_output");
      return;
   }

   for(int i=N_LAYER_SHELL_LAYERS-1; i>=0; i--){
      // process exclusive-zone clients from top to bottom
      arrange_layer(&output->layer_shells[i], &full_area, &usable_area, /* exclusive */ true);
   }

   output -> usable_area = usable_area;
   say(INFO, "Useable area = %dx%d+%d+%d / Full area = %dx%d+%d+%d", 
         usable_area.width, usable_area.height, usable_area.x, usable_area.y,
         full_area.width, full_area.height, full_area.x, full_area.y);

   for(int i=0; i<N_LAYER_SHELL_LAYERS; i++){
      arrange_layer(&output->layer_shells[i], &full_area, &usable_area, /* exclusive */ false);

      // set node position to account for output layout change
      //wlr_scene_node_set_position(&server->layer_tree[i]->node, scene_output->x, scene_output->y);
   }

   /*
   struct simple_layer_surface *lsurface;
   wl_list_for_each_reverse(lsurface, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP], link){
      say(DEBUG, "Present");
      input_focus_surface(server, lsurface->scene_layer_surface->layer_surface->surface);
      
   }*/
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

   struct simple_layer_surface *lsurface = wl_container_of(listener, lsurface, unmap);
   struct wlr_layer_surface_v1 *wlr_lsurface = lsurface->scene_layer_surface->layer_surface;

   lsurface->mapped = 0;
   wlr_scene_node_set_enabled(&lsurface->scene_tree->node, 0);
   //if(lsurface == exclusive_focus)
   //   exclusive_focus=NULL;
   if(wlr_lsurface->output && (lsurface->output = wlr_lsurface->output->data))
      arrange_layers(lsurface->output);
   if(wlr_lsurface->surface == lsurface->server->seat->keyboard_state.focused_surface)
      focus_client(get_top_client_from_output(lsurface->server->cur_output), wlr_lsurface->surface, false);
}

static void 
layer_surface_commit_notify(struct wl_listener *listener, void *data) 
{
   //
   say(INFO, "layer_surface_commit_notify");

   struct simple_layer_surface *lsurface = wl_container_of(listener, lsurface, surface_commit);
   struct wlr_layer_surface_v1 *wlr_lsurface = lsurface->scene_layer_surface->layer_surface;
   struct wlr_output *wlr_output = wlr_lsurface->output;
   struct wlr_scene_tree *layer = lsurface->server->layer_tree[layermap[wlr_lsurface->current.layer]];

   // For some reason this surface has no output, its monitor has just been destroyed
   if(!wlr_output || !(lsurface->output = wlr_output->data))
      return;

   if(layer != lsurface->scene_tree->node.parent) {
      wlr_scene_node_reparent(&lsurface->scene_tree->node, layer);
      wlr_scene_node_reparent(&lsurface->popups->node, layer);
      wl_list_remove(&lsurface->link);
      wl_list_insert(&lsurface->output->layer_shells[wlr_lsurface->current.layer], &lsurface->link);
   }
   //if (wlr_lsurface->current.layer <= ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM){
   //   wlr_scene_node_reparent(&lsurface->popups->node, lsurface->server->layer_tree[LyrTop]);
   //}

   if(wlr_lsurface->current.committed==0 && lsurface->mapped == wlr_lsurface->surface->mapped)
      return;
   lsurface->mapped = wlr_lsurface->surface->mapped;

   arrange_layers(lsurface->output);
}

static void 
layer_surface_destroy_notify(struct wl_listener *listener, void *data) 
{
   //
   say(INFO, "layer_surface_destroy_notify");
   struct simple_layer_surface *lsurface = wl_container_of(listener, lsurface, destroy);

   wl_list_remove(&lsurface->link);
   wl_list_remove(&lsurface->destroy.link);
   wl_list_remove(&lsurface->map.link);
   wl_list_remove(&lsurface->unmap.link);
   wl_list_remove(&lsurface->surface_commit.link);
   //wlr_scene_node_destroy(&lsurface->scene_tree->node);
   free(lsurface);
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
   struct wlr_scene_tree *selected_layer = server->layer_tree[layermap[layer_surface->pending.layer]];

   struct simple_layer_surface *lsurface = calloc(1, sizeof(struct simple_layer_surface));
   lsurface->type = LAYER_SHELL_CLIENT;
   lsurface->server = server;
   lsurface->output = output;

   lsurface->scene_layer_surface = wlr_scene_layer_surface_v1_create(selected_layer, layer_surface);
   lsurface->scene_layer_surface->layer_surface = layer_surface;
   lsurface->scene_tree = lsurface->scene_layer_surface->tree;
   lsurface->popups = layer_surface->surface->data = wlr_scene_tree_create(selected_layer);
   layer_surface->data = lsurface;
   lsurface->scene_tree->node.data = lsurface;

   LISTEN(&layer_surface->surface->events.map, &lsurface->map, layer_surface_map_notify);
   LISTEN(&layer_surface->surface->events.unmap, &lsurface->unmap, layer_surface_unmap_notify);
   LISTEN(&layer_surface->surface->events.commit, &lsurface->surface_commit, layer_surface_commit_notify);
   LISTEN(&layer_surface->events.destroy, &lsurface->destroy, layer_surface_destroy_notify);

   wl_list_insert(&output->layer_shells[layer_surface->pending.layer], &lsurface->link);

   struct wlr_layer_surface_v1_state old_state = layer_surface->current;
   layer_surface->current = layer_surface->pending;
   lsurface->mapped = 1;
   arrange_layers(output);
   layer_surface->current = old_state;
}
