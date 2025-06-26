#include <string.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_seat.h>

#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>

#include "globals.h"
#include "client.h"
#include "server.h"
#include "output.h"
#include "input.h"
#include "layer.h"
#include "ipc.h"

//------------------------------------------------------------------------
void
arrange_outputs()
{
   say(DEBUG, "arrange_outputs");
   struct simple_client* client, *focused_client=NULL;

   get_client_from_surface(g_server->seat->keyboard_state.focused_surface, &focused_client, NULL);
   
   if(focused_client)
      wlr_scene_node_set_enabled(&focused_client->output->fullscreen_bg->node, focused_client->fullscreen);

   int n=0;
   bool is_client_visible=false;
   wl_list_for_each(client, &g_server->clients, link) {
      if(client->destroy_requested) continue;

      if(client->output->fixed_tag<0)
         is_client_visible = (client->visible && (client->fixed || (client->tag & g_server->visible_tags)));
      else
         is_client_visible = (client->visible && (client->fixed || (client->tag & client->output->fixed_tag)));

      if(is_client_visible) n++;
      set_client_border_colour(client, client==focused_client ? FOCUSED : UNFOCUSED);
      wlr_scene_node_set_enabled(&client->scene_tree->node, is_client_visible);
   }

   focused_client = get_top_client_from_output(g_server->cur_output, false);
   if(focused_client)
      focus_client(focused_client, true);
   else
      input_focus_surface(NULL);

   check_idle_inhibitor();
}

struct simple_output*
get_output_at(double x, double y)
{
   double closest_x, closest_y;
   wlr_output_layout_closest_point(g_server->output_layout, NULL, x, y, 
         &closest_x, &closest_y);
   struct wlr_output *output = wlr_output_layout_output_at(g_server->output_layout, closest_x, closest_y);
   if(!output) return NULL;

   struct simple_output* test_output;
   wl_list_for_each(test_output, &g_server->outputs, link) {
      if(test_output && output->data == test_output) return test_output;
   }
   return NULL;
}

void
toggleFixedTag(){
   struct simple_output* output = g_server->cur_output;

   if(output->fixed_tag<0) {
      output->fixed_tag = g_server->current_tag;

      // draw the border
      int bw = g_config->border_width;
      struct simple_outline* outline = simple_outline_create(&g_server->scene->tree, g_config->border_colour[FIXED], bw);
      output->outline = outline;
      wlr_scene_node_place_above(&outline->tree->node, &g_server->layer_tree[LyrBottom]->node);

      simple_outline_set_size(outline, output->usable_area.width-2*bw, output->usable_area.height-2*bw);
      wlr_scene_node_set_position(&outline->tree->node, output->usable_area.x+bw, output->usable_area.y+bw);
      //---
   } else {
      output->fixed_tag = -1;

      wlr_scene_node_destroy(&output->outline->tree->node);
   }
}

//--- Output notify functions --------------------------------------------
static void 
output_frame_notify(struct wl_listener *listener, void *data) 
{
   //say(DEBUG, "output_frame_notify");
   struct simple_output *output = wl_container_of(listener, output, frame);
   struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(g_server->scene, output->wlr_output);
   
   // Render the scene if needed and commit the output 
   wlr_scene_output_commit(scene_output, NULL);
   
   struct timespec now;
   clock_gettime(CLOCK_MONOTONIC, &now);
   wlr_scene_output_send_frame_done(scene_output, &now);
}

static void 
output_request_state_notify(struct wl_listener *listener, void *data) 
{
   say(DEBUG, "output_request_state_notify");
   // called when the backend requests a new state for the output
   struct simple_output *output = wl_container_of(listener, output, request_state);
   const struct wlr_output_event_request_state *event = data;
   wlr_output_commit_state(output->wlr_output, event->state);
}

static void 
output_destroy_notify(struct wl_listener *listener, void *data) 
{
   say(DEBUG, "output_destroy_notify");
   struct simple_output *output = wl_container_of(listener, output, destroy);

   struct simple_ipc_output *ipc_output, *ipc_output_tmp;
   wl_list_for_each_safe(ipc_output, ipc_output_tmp, &output->ipc_outputs, link)
      wl_resource_destroy(ipc_output->resource);

   struct simple_layer_surface *l, *tmp;
   size_t i;

   for(i=0; i<LENGTH(output->layer_shells); i++){
      wl_list_for_each_safe(l, tmp, &output->layer_shells[i], link)
         wlr_layer_surface_v1_destroy(l->scene_layer_surface->layer_surface);
   }

   wl_list_remove(&output->frame.link);
   wl_list_remove(&output->request_state.link);
   wl_list_remove(&output->destroy.link);
   wl_list_remove(&output->link);
   output->wlr_output->data = NULL;
   wlr_output_layout_remove(g_server->output_layout, output->wlr_output);
   //wlr_scene_output_destroy(output->scene_output);

   struct simple_output *test_output;
   wl_list_for_each(test_output, &g_server->outputs, link) {
      if(test_output == output) continue;
      if(test_output->wlr_output->enabled) g_server->cur_output = test_output;
   }

   // Move clients to the previous output
   struct simple_client * client;
   wl_list_for_each(client, &g_server->clients, link) {
      if(client->output != output) continue;

      if(client->geom.x > output->usable_area.x){
         client->geom.x = client->geom.x - output->usable_area.width;
         set_client_geometry(client, false);
      }
      struct simple_output *new_op = get_output_at(client->geom.x, client->geom.y);
      if(new_op->wlr_output->enabled && new_op==g_server->cur_output) 
         client->output = new_op;
   
      //client->output = g_server->cur_output;
   }

   wlr_scene_node_destroy(&output->fullscreen_bg->node);
   free(output);

   print_server_info();
}

//------------------------------------------------------------------------
void 
output_layout_change_notify(struct wl_listener *listener, void *data) 
{
   // Called when the output layout changes: e.g. adding/removing a monitor
   say(DEBUG, "output_layout_change_notify");

   struct wlr_output_configuration_v1 *config = wlr_output_configuration_v1_create();
   struct wlr_output_configuration_head_v1 *config_head;
   if(!config)
      say(ERROR, "wlr_output_configuration_v1_create failed");

   struct simple_output *output;

   wl_list_for_each(output, &g_server->outputs, link) {
      if(!output->wlr_output->enabled) continue;

      config_head = wlr_output_configuration_head_v1_create(config, output->wlr_output);
      if(!config_head) {
         wlr_output_configuration_v1_destroy(config);
         say(ERROR, "wlr_output_configuration_head_v1_create failed");
      }

      struct wlr_box box;
      wlr_output_layout_get_box(g_server->output_layout, output->wlr_output, &box);
      if(wlr_box_empty(&box))
         say(ERROR, "Failed to get output layout box");

      memset(&output->usable_area, 0, sizeof(output->usable_area));
      memset(&output->full_area, 0, sizeof(output->full_area));
      output->usable_area = output->full_area = box;

      arrange_layers(output);
      arrange_outputs();

      //output->gamma_lut_changed = true;
      config_head->state.x = box.x;
      config_head->state.y = box.y;
   }

   if(config && g_server->output_manager)
      wlr_output_manager_v1_set_configuration(g_server->output_manager, config);
   else
      say(ERROR, "wlr_output_manager_v1_set_configuration failed");
}

void 
new_output_notify(struct wl_listener *listener, void *data) 
{
   say(DEBUG, "new_output_notify");
   struct wlr_output *wlr_output = data;

   // Don't configure any non-desktop displays, such as VR headsets
   if(wlr_output->non_desktop) {
      say(DEBUG, "Not configuring non-desktop output");
      return;
   }

   // Configures the output created by the backend to use the allocator and renderer
   // Must be done once, before committing the output
   if(!wlr_output_init_render(wlr_output, g_server->allocator, g_server->renderer))
      say(ERROR, "unable to initialize output renderer");
   
   // The output may be disabled. Switch it on
   struct wlr_output_state state;
   wlr_output_state_init(&state);
   wlr_output_state_set_enabled(&state, true);

   struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
   if (mode)
      wlr_output_state_set_mode(&state, mode);

   wlr_output_commit_state(wlr_output, &state);
   wlr_output_state_finish(&state);

   struct simple_output *output = calloc(1, sizeof(struct simple_output));
   output->wlr_output = wlr_output;
   wlr_output->data = output;

   // set default tag - do this before adding the current output to the global list
   output->fixed_tag = -1;

   wl_list_init(&output->ipc_outputs);   // ipc addition

   output->fullscreen_bg = wlr_scene_rect_create(g_server->layer_tree[LyrFS], 0, 0, (float [4]){0.1, 0.1, 0.1, 1.0});
   wlr_scene_node_set_enabled(&output->fullscreen_bg->node, 0);

   LISTEN(&wlr_output->events.frame, &output->frame, output_frame_notify);
   LISTEN(&wlr_output->events.destroy, &output->destroy, output_destroy_notify);
   LISTEN(&wlr_output->events.request_state, &output->request_state, output_request_state_notify);

   wl_list_insert(&g_server->outputs, &output->link);

   for(int i=0; i<N_LAYER_SHELL_LAYERS; i++)
      wl_list_init(&output->layer_shells[i]);
   
   wlr_scene_node_lower_to_bottom(&g_server->layer_tree[LyrBottom]->node);
   wlr_scene_node_lower_to_bottom(&g_server->layer_tree[LyrBg]->node);
   wlr_scene_node_raise_to_top(&g_server->layer_tree[LyrTop]->node);
   wlr_scene_node_raise_to_top(&g_server->layer_tree[LyrFS]->node);
   wlr_scene_node_raise_to_top(&g_server->layer_tree[LyrOverlay]->node);
   wlr_scene_node_raise_to_top(&g_server->layer_tree[LyrLock]->node);


   struct wlr_output_layout_output *l_output =
      wlr_output_layout_add_auto(g_server->output_layout, wlr_output);
   struct wlr_scene_output *scene_output =
      wlr_scene_output_create(g_server->scene, wlr_output);
   wlr_scene_output_layout_add_output(g_server->scene_output_layout, l_output, scene_output);

   // update background and lock geometry
   struct wlr_box geom;
   wlr_output_layout_get_box(g_server->output_layout, NULL, &geom);

   wlr_scene_node_set_position(&g_server->root_bg->node, geom.x, geom.y);
   wlr_scene_rect_set_size(g_server->root_bg, geom.width, geom.height);
   wlr_scene_node_set_position(&g_server->locked_bg->node, geom.x, geom.y);
   wlr_scene_rect_set_size(g_server->locked_bg, geom.width, geom.height);

   wlr_scene_node_set_position(&output->fullscreen_bg->node, output->usable_area.x, output->usable_area.y);
   wlr_scene_rect_set_size(output->fullscreen_bg, output->usable_area.width, output->usable_area.height);

   wlr_scene_node_set_enabled(&g_server->root_bg->node, 1);

   print_server_info();

   say(INFO, " -> Output %s : %dx%d+%d+%d", l_output->output->name,
         output->usable_area.width, output->usable_area.height, 
         output->usable_area.x, output->usable_area.y);
}

