#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <linux/input-event-codes.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/edges.h>
//
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
//
#include <wlr/types/wlr_input_method_v2.h>
#include <wlr/types/wlr_text_input_v3.h>
//#include <wlr/types/wlr_foreign_toplevel_management_v1.h>

#include <wlr/util/log.h>
#if XWAYLAND
#include <wlr/xwayland.h>
#endif

#include "dwl-ipc-unstable-v2-protocol.h"
#include "globals.h"
#include "layer.h"
#include "client.h"
#include "server.h"
#include "action.h"
#include "ipc.h"


//--- client outline procedures ------------------------------------------
static void
client_outline_destroy_notify(struct wl_listener *listener, void *data)
{
   struct client_outline* outline = wl_container_of(listener, outline, destroy);
   wl_list_remove(&outline->destroy.link);
   free(outline);
}

struct client_outline*
client_outline_create(struct wlr_scene_tree *parent, float* border_colour, int line_width)
{
   struct client_outline* outline = calloc(1, sizeof(struct client_outline));
   outline->line_width = line_width;
   outline->tree = wlr_scene_tree_create(parent);

   outline->top = wlr_scene_rect_create(outline->tree, 0, 0, border_colour);
   outline->bottom = wlr_scene_rect_create(outline->tree, 0, 0, border_colour);
   outline->left = wlr_scene_rect_create(outline->tree, 0, 0, border_colour);
   outline->right = wlr_scene_rect_create(outline->tree, 0, 0, border_colour);

   LISTEN(&outline->tree->node.events.destroy, &outline->destroy, client_outline_destroy_notify);
   
   return outline;
}

void
client_outline_set_size(struct client_outline* outline, int width, int height) {
   //borders
   int bw = outline->line_width;
   //top
   wlr_scene_rect_set_size(outline->top, width, bw);
   wlr_scene_node_set_position(&outline->top->node, 0, -bw);
   //bottom
   wlr_scene_rect_set_size(outline->bottom, width, bw);
   wlr_scene_node_set_position(&outline->bottom->node, 0, height);
   //left
   wlr_scene_rect_set_size(outline->left, bw, height + 2 * bw);
   wlr_scene_node_set_position(&outline->left->node, -bw, -bw);
   //right
   wlr_scene_rect_set_size(outline->right, bw, height + 2 * bw);
   wlr_scene_node_set_position(&outline->right->node, width, -bw);
}

//------------------------------------------------------------------------
void
setCurrentTag(struct simple_server* server, int tag, bool toggle)
{
   say(DEBUG, "setCurrentTag %d", tag);
   struct simple_output* output = server->cur_output;
   if(toggle)
      output->visible_tags ^= TAGMASK(tag);
   else 
      output->visible_tags = output->current_tag = TAGMASK(tag);

   arrange_output(output);
}

void
tileTag(struct simple_server* server) 
{
   struct simple_client* client;
   struct simple_output* output = server->cur_output;
   
   // first count the number of clients
   int n=0;
   wl_list_for_each(client, &server->clients, link){
      if(!VISIBLEON(client, output)) continue;
      n++;
   }

   int gap_width = server->config->tile_gap_width;
   int bw = server->config->border_width;

   int i=0;
   struct wlr_box new_geom;
   wl_list_for_each(client, &server->clients, link){
      if(!VISIBLEON(client, output)) continue;
      
      if(i==0) { // master window
         new_geom.x = output->usable_area.x + gap_width + bw;
         new_geom.y = output->usable_area.y + gap_width + bw;
         new_geom.width = (output->usable_area.width - (gap_width*(MIN(2,n)+1)))/MIN(2,n) - bw*2;
         new_geom.height = output->usable_area.height - gap_width*2 - bw*2;
         
         set_client_geometry(client, new_geom);
      } else {
         new_geom.x = output->usable_area.width/2 + gap_width/2 + bw;
         new_geom.width = (output->usable_area.width - (gap_width*3))/2 - bw*2;
         new_geom.height = (output->usable_area.height - (gap_width*n))/(n-1);
         new_geom.y = output->usable_area.y + (gap_width*i) + (new_geom.height*(i-1)) + bw;
         new_geom.height -= 2*bw;
         
         set_client_geometry(client, new_geom);
      }
      i++;
   }
   arrange_output(output);
}

struct simple_output*
get_output_at(struct simple_server* server, double x, double y)
{
   struct wlr_output *output = wlr_output_layout_output_at(server->output_layout, x, y);
   return output ? output->data : NULL;
}

void
print_server_info(struct simple_server* server) 
{
   struct simple_output* output;
   struct simple_client* client;

   wl_list_for_each(output, &server->outputs, link) {
      say(DEBUG, "output %s", output->wlr_output->name);
      say(DEBUG, " -> cur_output = %u", output == server->cur_output);
      say(DEBUG, " -> tag = vis:%u / cur:%u", output->visible_tags, output->current_tag);
      wl_list_for_each(client, &server->clients, link) {
         say(DEBUG, " -> client");
         say(DEBUG, "    -> client tag = %u", client->tag);
         say(DEBUG, "    -> client fixed = %b", client->fixed);
      }
   }

   wl_list_for_each(output, &server->outputs, link)
      ipc_output_printstatus(output);
}

void
check_idle_inhibitor(struct simple_server* server)
{
   int inhibited=0, lx, ly;
   struct wlr_idle_inhibitor_v1 *inhibitor;
   bool bypass_surface_visibility = 0; // 1 means idle inhibitors will disable idle tracking 
                                       // even if its surface isn't visible (from dwl)
   wl_list_for_each(inhibitor, &server->idle_inhibit_manager->inhibitors, link) {
      struct wlr_surface *surface = wlr_surface_get_root_surface(inhibitor->surface);
      struct wlr_scene_tree *tree = surface->data;
      if(surface && (bypass_surface_visibility || 
               (!tree || wlr_scene_node_coords(&tree->node, &lx, &ly)) )) {
         inhibited = 1;
         break;
      }
   }
   wlr_idle_notifier_v1_set_inhibited(server->idle_notifier, inhibited);
}

void
arrange_output(struct simple_output* output)
{
   say(DEBUG, "arrange_output");
   struct simple_server* server = output->server;
   struct simple_client* client, *focused_client;

   get_client_from_surface(server->seat->keyboard_state.focused_surface, &focused_client, NULL);
   
   wl_list_for_each(client, &server->clients, link) {
      if(client->output == output){
         set_client_border_colour(client, client==focused_client ? FOCUSED : UNFOCUSED);
         wlr_scene_node_set_enabled(&client->scene_tree->node, VISIBLEON(client, output));
      }
   }
   print_server_info(server);
   check_idle_inhibitor(server);
}


//--- Other notify functions ---------------------------------------------
static void
new_decoration_notify(struct wl_listener *listener, void *data)
{
   struct wlr_xdg_toplevel_decoration_v1 *decoration = data;
   wlr_xdg_toplevel_decoration_v1_set_mode(decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

static void
inhibitor_destroy_notify(struct wl_listener *listener, void *data)
{
   //
   say(DEBUG, "inhibitor_destroy_notify");
   struct simple_server *server = wl_container_of(listener, server, inhibitor_destroy);

   wlr_idle_notifier_v1_set_inhibited(server->idle_notifier, 0);
}

static void
new_inhibitor_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "new_inhibitor_notify");
   struct simple_server *server = wl_container_of(listener, server, new_inhibitor);
   struct wlr_idle_inhibitor_v1 *inhibitor = data;

   LISTEN(&inhibitor->events.destroy, &server->inhibitor_destroy, inhibitor_destroy_notify);
   check_idle_inhibitor(server);
}

static void
output_pm_set_mode_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "output_pm_set_mode_notify");
   struct simple_server *server = wl_container_of(listener, server, output_pm_set_mode);
   struct wlr_output_power_v1_set_mode_event *event = data;

   switch (event->mode) {
      case ZWLR_OUTPUT_POWER_V1_MODE_OFF:
         wlr_output_enable(event->output, false);
         wlr_output_commit(event->output);
         break;
      case ZWLR_OUTPUT_POWER_V1_MODE_ON:
         wlr_output_enable(event->output, true);
         if(!wlr_output_test(event->output))
            wlr_output_rollback(event->output);
         wlr_output_commit(event->output);

         // reset the cursor image
         //cursor_image(&server->seat);
         wlr_cursor_unset_image(server->cursor);
         wlr_cursor_set_xcursor(server->cursor, server->cursor_manager, "left_ptr");;
         break;
   }
}

//--- Lock session notify functions --------------------------------------
static void
lock_surface_destroy_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "lock_surface_destroy_notify");
   struct simple_output *output = wl_container_of(listener, output, lock_surface_destroy);
   struct wlr_session_lock_surface_v1 *lock_surface = output->lock_surface;
   struct simple_server *server = output->server;

   output->lock_surface = NULL;
   wl_list_remove(&output->lock_surface_destroy.link);

   if(lock_surface->surface != server->seat->keyboard_state.focused_surface)
      return;

   if(server->locked && server->cur_lock && !wl_list_empty(&server->cur_lock->surfaces)){
      struct wlr_session_lock_surface_v1 *surface = wl_container_of(server->cur_lock->surfaces.next, surface, link);
      input_focus_surface(server, surface->surface);
   } else if(!(server->locked)){
      //focus_client();
   } else {
      wlr_seat_keyboard_clear_focus(server->seat);
   }
}

static void
new_lock_surface_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "new_lock_surface_notify");
   struct simple_session_lock *slock = wl_container_of(listener, slock, new_surface);
   struct simple_server *server = slock->server;
   struct wlr_session_lock_surface_v1 *lock_surface = data;
   struct simple_output *output = lock_surface->output->data;
   struct wlr_scene_tree *scene_tree = wlr_scene_subsurface_tree_create(slock->scene, lock_surface->surface);
   lock_surface->surface->data = scene_tree;
   output->lock_surface = lock_surface;
   
   wlr_scene_node_set_position(&scene_tree->node, output->full_area.x, output->full_area.y);
   wlr_session_lock_surface_v1_configure(lock_surface, output->full_area.width, output->full_area.height);

   LISTEN(&lock_surface->events.destroy, &output->lock_surface_destroy, lock_surface_destroy_notify);
   
   if(output == server->cur_output)
      input_focus_surface(server, lock_surface->surface);
}

static void
unlock_session_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "unlock_session_notify");
   struct simple_session_lock *slock = wl_container_of(listener, slock, unlock);
   struct simple_server *server = slock->server;
   
   //destroylock(lock, 1);
   server->locked = false;
   wlr_seat_keyboard_notify_clear_focus(server->seat);

   wlr_scene_node_set_enabled(&server->locked_bg->node, 0);
   //focus_client()
}

static void
lock_session_destroy_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "lock_session_destroy_notify");
   struct simple_session_lock *slock = wl_container_of(listener, slock, destroy);
   struct simple_server *server = slock->server;

   //destroylock(lock, 0);
   wlr_seat_keyboard_notify_clear_focus(server->seat);

   wl_list_remove(&slock->new_surface.link);
   wl_list_remove(&slock->unlock.link);
   wl_list_remove(&slock->destroy.link);

   wlr_scene_node_destroy(&slock->scene->node);
   server->cur_lock = NULL;
   free(slock);
}

static void
lock_session_manager_destroy_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "lock_session_manager_destroy_notify");
   struct simple_server* server = wl_container_of(listener, server, lock_session_manager_destroy);

   wl_list_remove(&server->new_lock_session_manager.link);
   wl_list_remove(&server->lock_session_manager_destroy.link);
}

static void
new_lock_session_manager_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "new_lock_session_manager_notify");
   struct simple_server* server = wl_container_of(listener, server, new_lock_session_manager); 
   struct wlr_session_lock_v1 *session_lock = data;

   wlr_scene_node_set_enabled(&server->locked_bg->node, 1);
   if(server->cur_lock){
      wlr_session_lock_v1_destroy(session_lock);
      return;
   }

   //focusclient(NULL, 0);
   struct simple_session_lock *slock = calloc(1, sizeof(struct simple_session_lock));
   slock->scene = wlr_scene_tree_create(server->layer_tree[LyrLock]);
   server->cur_lock = slock->lock = session_lock;
   server->locked = true;
   session_lock->data = slock;
   slock->server = server;

   LISTEN(&session_lock->events.new_surface, &slock->new_surface, new_lock_surface_notify);
   LISTEN(&session_lock->events.unlock, &slock->unlock, unlock_session_notify);
   LISTEN(&session_lock->events.destroy, &slock->destroy, lock_session_destroy_notify);

   wlr_session_lock_v1_send_locked(session_lock);
}

//--- Output notify functions --------------------------------------------
static void 
output_layout_change_notify(struct wl_listener *listener, void *data) 
{
   say(DEBUG, "output_layout_change_notify");
   struct simple_server *server = wl_container_of(listener, server, output_layout_change);

   struct wlr_output_configuration_v1 *config = wlr_output_configuration_v1_create();
   struct wlr_output_configuration_head_v1 *config_head;
   if(!config)
      say(ERROR, "wlr_output_configuration_v1_create failed");

   struct simple_output *output;
   wl_list_for_each(output, &server->outputs, link) {
      if(!output->wlr_output->enabled) continue;

      config_head = wlr_output_configuration_head_v1_create(config, output->wlr_output);
      if(!config_head) {
         wlr_output_configuration_v1_destroy(config);
         say(ERROR, "wlr_output_configuration_head_v1_create failed");
      }
      struct wlr_box box;
      wlr_output_layout_get_box(server->output_layout, output->wlr_output, &box);
      if(wlr_box_empty(&box))
         say(ERROR, "Failed to get output layout box");

      memset(&output->usable_area, 0, sizeof(output->usable_area));
      output->usable_area = box;

      config_head->state.x = box.x;
      config_head->state.y = box.y;
   }

   if(config)
      wlr_output_manager_v1_set_configuration(server->output_manager, config);
   else
      say(ERROR, "wlr_output_manager_v1_set_configuration failed");
}

static void 
output_manager_apply_notify(struct wl_listener *listener, void *data) 
{
   say(DEBUG, "output_manager_apply_notify");
   //
}

static void 
output_manager_test_notify(struct wl_listener *listener, void *data) 
{
   say(DEBUG, "output_manager_test_notify");
   //
}

static void 
output_frame_notify(struct wl_listener *listener, void *data) 
{
   //say(DEBUG, "output_frame_notify");
   struct simple_output *output = wl_container_of(listener, output, frame);
   struct wlr_scene *scene = output->server->scene;
   struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(scene, output->wlr_output);
   
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

   wl_list_remove(&output->frame.link);
   wl_list_remove(&output->request_state.link);
   wl_list_remove(&output->destroy.link);
   wl_list_remove(&output->link);
   free(output);
}

//------------------------------------------------------------------------
static void 
new_output_notify(struct wl_listener *listener, void *data) 
{
   say(DEBUG, "new_output_notify");
   
   struct simple_server *server = wl_container_of(listener, server, new_output);
   struct wlr_output *wlr_output = data;

   // Don't configure any non-desktop displays, such as VR headsets
   if(wlr_output->non_desktop) {
      say(DEBUG, "Not configuring non-desktop output");
      return;
   }

   // Configures the output created by the backend to use the allocator and renderer
   // Must be done once, before committing the output
   if(!wlr_output_init_render(wlr_output, server->allocator, server->renderer))
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
   output->server = server;

   wl_list_init(&output->ipc_outputs);   // ipc addition

   LISTEN(&wlr_output->events.frame, &output->frame, output_frame_notify);
   LISTEN(&wlr_output->events.destroy, &output->destroy, output_destroy_notify);
   LISTEN(&wlr_output->events.request_state, &output->request_state, output_request_state_notify);

   wl_list_insert(&server->outputs, &output->link);
   

   for(int i=0; i<N_LAYER_SHELL_LAYERS; i++)
      wl_list_init(&output->layer_shells[i]);
   
   wlr_scene_node_lower_to_bottom(&server->layer_tree[LyrBottom]->node);
   wlr_scene_node_lower_to_bottom(&server->layer_tree[LyrBg]->node);
   wlr_scene_node_raise_to_top(&server->layer_tree[LyrTop]->node);
   wlr_scene_node_raise_to_top(&server->layer_tree[LyrOverlay]->node);
   wlr_scene_node_raise_to_top(&server->layer_tree[LyrLock]->node);

   //set default tag
   output->current_tag = TAGMASK(0);
   output->visible_tags = TAGMASK(0);

   struct wlr_output_layout_output *l_output =
      wlr_output_layout_add_auto(server->output_layout, wlr_output);
   struct wlr_scene_output *scene_output =
      wlr_scene_output_create(server->scene, wlr_output);
   wlr_scene_output_layout_add_output(server->scene_output_layout, l_output, scene_output);

   // update background and lock geometry
   struct wlr_box geom;
   wlr_output_layout_get_box(server->output_layout, NULL, &geom);

   memset(&output->full_area, 0, sizeof(output->full_area));
   output->full_area = geom;
   
   wlr_scene_node_set_position(&server->root_bg->node, geom.x, geom.y);
   wlr_scene_rect_set_size(server->root_bg, geom.width, geom.height);
   wlr_scene_node_set_position(&server->locked_bg->node, geom.x, geom.y);
   wlr_scene_rect_set_size(server->locked_bg, geom.width, geom.height);

   wlr_scene_node_set_enabled(&server->root_bg->node, 1);
   //

   say(INFO, " -> Output %s : %dx%d+%d+%d", l_output->output->name,
         l_output->output->width, l_output->output->height, 
         l_output->x, l_output->y);
}

//--- Input functions ----------------------------------------------------
void 
input_focus_surface(struct simple_server *server, struct wlr_surface *surface) 
{
   if(!surface) {
      wlr_seat_keyboard_notify_clear_focus(server->seat);
      return;
   }
   struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
      
   wlr_seat_keyboard_notify_enter(server->seat, surface, keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
}

//--- Keyboard events ----------------------------------------------------
static void 
kb_modifiers_notify(struct wl_listener *listener, void *data) 
{
   struct simple_input *keyboard = wl_container_of(listener, keyboard, kb_modifiers);
   struct simple_server *server = keyboard->server;
   struct wlr_keyboard* wlr_kb = keyboard->keyboard;

   if(keyboard->server->grabbed_client) {
      // we are still cycling through the windows
      xkb_mod_index_t i;
      bool mod_pressed = false;
      for(i=0; i<xkb_keymap_num_mods(wlr_kb->keymap); i++){
         if(xkb_state_mod_index_is_active(wlr_kb->xkb_state, i, XKB_STATE_MODS_DEPRESSED))
            mod_pressed = true;
      }
      if(!mod_pressed) {
         struct simple_client* client = keyboard->server->grabbed_client;
         if(server->grabbed_client_outline){
            wlr_scene_node_destroy(&server->grabbed_client_outline->tree->node);
            server->grabbed_client_outline=NULL;
         }

         // change stacking order and focus client 
         wl_list_remove(&client->link);
         wl_list_insert(&client->server->clients, &client->link);
         focus_client(client, true);
         keyboard->server->grabbed_client=NULL;
         arrange_output(server->cur_output);
      }
   }

   wlr_seat_keyboard_notify_modifiers(keyboard->server->seat, &keyboard->keyboard->modifiers);
}

static void 
kb_key_notify(struct wl_listener *listener, void *data) 
{
   struct simple_input *keyboard = wl_container_of(listener, keyboard, kb_key);
   struct simple_server *server = keyboard->server;
   struct wlr_keyboard_key_event *event = data;

   uint32_t keycode = event->keycode + 8;
   const xkb_keysym_t *syms;
   int nsyms = xkb_state_key_get_syms(keyboard->keyboard->xkb_state, keycode, &syms);
   
   bool handled = false;
   uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->keyboard);

   wlr_idle_notifier_v1_notify_activity(server->idle_notifier, server->seat);

   if(event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
      for(int i=0; i<nsyms; i++){
         struct keymap *keymap;
         wl_list_for_each(keymap, &server->config->key_bindings, link) {
            if (modifiers ^ keymap->mask) continue;

            if (syms[i] == keymap->keysym){
               key_function(server, keymap);
               handled=true;
            }
         }
      }
   }

   if(!handled) {
      wlr_seat_set_keyboard(server->seat, keyboard->keyboard);
      wlr_seat_keyboard_notify_key(server->seat, event->time_msec, event->keycode, event->state);
   }
}

//--- Pointer events -----------------------------------------------------
static uint32_t 
get_resize_edges(struct simple_client *client, double x, double y) 
{
   uint32_t edges = 0;

   struct wlr_box box = client->geom;
   edges |= x < (box.x + box.width/2)  ? WLR_EDGE_LEFT : WLR_EDGE_RIGHT;
   edges |= y < (box.y + box.height/2) ? WLR_EDGE_TOP : WLR_EDGE_BOTTOM;
   return edges;
}

static void 
process_cursor_move(struct simple_server *server, uint32_t time) 
{
   struct simple_client *client = server->grabbed_client;
   client->geom.x = server->cursor->x - server->grab_x;
   client->geom.y = server->cursor->y - server->grab_y;
   wlr_scene_node_set_position(&client->scene_tree->node, client->geom.x, client->geom.y);
}

static void 
process_cursor_resize(struct simple_server *server, uint32_t time) 
{
   struct simple_client *client = server->grabbed_client;
   
   double delta_x = server->cursor->x - server->grab_x;
   double delta_y = server->cursor->y - server->grab_y;
   int new_left = server->grab_box.x;
   int new_right = server->grab_box.x + server->grab_box.width;
   int new_top = server->grab_box.y;
   int new_bottom = server->grab_box.y + server->grab_box.height;
   
   if (server->resize_edges & WLR_EDGE_TOP) {
      new_top += delta_y;
      if(new_top >= new_bottom)
         new_top = new_bottom - 1;
   } else if (server->resize_edges & WLR_EDGE_BOTTOM) {
      new_bottom += delta_y;
      if(new_bottom <= new_top)
         new_bottom = new_top + 1;
   }
   
   if (server->resize_edges & WLR_EDGE_LEFT) {
      new_left += delta_x;
      if(new_left >= new_right)
         new_left = new_right - 1;
   } else if (server->resize_edges & WLR_EDGE_RIGHT) {
      new_right += delta_x;
      if(new_right <= new_left)
         new_right = new_left + 1;
   }

   client->geom.x = new_left;
   client->geom.y = new_top;
   client->geom.width = new_right - new_left;
   client->geom.height = new_bottom - new_top;

   set_client_geometry(client, client->geom);
}

static void 
process_cursor_motion(struct simple_server *server, uint32_t time) 
{
   //say(DEBUG, "process_cursor_motion");

   // time is 0 in internal calls meant to restore point focus
   if(time>0){
      wlr_idle_notifier_v1_notify_activity(server->idle_notifier, server->seat);
   }

   if(server->cursor_mode == CURSOR_MOVE) {
      process_cursor_move(server, time);
      return;
   } else if(server->cursor_mode == CURSOR_RESIZE) {
      process_cursor_resize(server, time);
      return;
   } 

   // Otherwise, find the client under the pointer and send the event along
   double sx, sy;
   struct wlr_seat *wlr_seat = server->seat;
   struct wlr_surface *surface = NULL;
   struct simple_client *client = get_client_at(server, server->cursor->x, server->cursor->y, &surface, &sx, &sy);
   
   if(!client)
      wlr_cursor_set_xcursor(server->cursor, server->cursor_manager, "left_ptr");

   if(client && surface && client->server->config->sloppy_focus){
      focus_client(client, false);
   }

   if(surface) {
      wlr_seat_pointer_notify_enter(wlr_seat, surface, sx, sy);
      wlr_seat_pointer_notify_motion(wlr_seat, time, sx, sy);
   } else
      wlr_seat_pointer_clear_focus(wlr_seat);
}

//--- cursor notify functions --------------------------------------------
static void 
cursor_motion_notify(struct wl_listener *listener, void *data) 
{
   //say(DEBUG, "cursor_motion_notify");
   struct simple_server *server = wl_container_of(listener, server, cursor_motion);
   struct wlr_pointer_motion_event *event = data;

   wlr_cursor_move(server->cursor, &event->pointer->base, event->delta_x, event->delta_y);
   process_cursor_motion(server, event->time_msec);
}

static void 
cursor_motion_abs_notify(struct wl_listener *listener, void *data) 
{
  // say(DEBUG, "cursor_motion_abs_notify");
   struct simple_server *server = wl_container_of(listener, server, cursor_motion_abs);
   struct wlr_pointer_motion_absolute_event *event = data;

   wlr_cursor_warp_absolute(server->cursor, &event->pointer->base, event->x, event->y);
   process_cursor_motion(server, event->time_msec);
}

static void 
cursor_button_notify(struct wl_listener *listener, void *data) 
{
   say(DEBUG, "cursor_button_notify");
   struct simple_server *server = wl_container_of(listener, server, cursor_button);
   struct wlr_pointer_button_event *event = data;
   
   // Notify the client with pointer focus that a button press has occurred
   wlr_seat_pointer_notify_button(server->seat, event->time_msec, event->button, event->state);

   wlr_idle_notifier_v1_notify_activity(server->idle_notifier, server->seat);

   if(server->locked) return;

   double sx, sy;
   struct wlr_surface *surface = NULL;
   //uint32_t resize_edges;
   struct simple_client *client = get_client_at(server, server->cursor->x, server->cursor->y, &surface, &sx, &sy);

   // button release
   if(event->state == WLR_BUTTON_RELEASED) {
      server->cursor_mode = CURSOR_PASSTHROUGH;
      server->grabbed_client = NULL;
      return;
   }
   
   // press on desktop
   if(!client) {
      say(DEBUG, "press on desktop");
      return;
   }
   
   //press on client 
   struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
   uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard);
   if((modifiers & WLR_MODIFIER_ALT) && (event->button == BTN_LEFT) ) {
      begin_interactive(client, CURSOR_MOVE, 0);
      return;
   } else if ((modifiers & WLR_MODIFIER_ALT) && (event->button == BTN_RIGHT) ) {
      uint32_t resize_edges = get_resize_edges(client, server->cursor->x, server->cursor->y);
      begin_interactive(client, CURSOR_RESIZE, resize_edges);
      return;
   }
   
   focus_client(client, true);
}

static void 
cursor_axis_notify(struct wl_listener *listener, void *data) 
{
   //say(DEBUG, "cursor_axis_notify");
   struct simple_server *server = wl_container_of(listener, server, cursor_axis);
   struct wlr_pointer_axis_event *event = data;

   wlr_idle_notifier_v1_notify_activity(server->idle_notifier, server->seat);
   wlr_seat_pointer_notify_axis(server->seat, event->time_msec, event->orientation, event->delta, event->delta_discrete, event->source);
}

static void 
cursor_frame_notify(struct wl_listener *listener, void *data) 
{
   //say(DEBUG, "cursor_frame_notify");
   struct simple_server *server = wl_container_of(listener, server, cursor_frame);

   wlr_seat_pointer_notify_frame(server->seat);
}

static void 
request_cursor_notify(struct wl_listener *listener, void *data) 
{
   //say(DEBUG, "seat_request_cursor_notify");
   struct simple_server *server = wl_container_of(listener, server, request_cursor);
   struct wlr_seat_pointer_request_set_cursor_event *event = data;
   struct wlr_seat_client *focused_client = server->seat->pointer_state.focused_client;
   
   if(focused_client == event->seat_client) {
      wlr_cursor_set_surface(server->cursor, event->surface, event->hotspot_x, event->hotspot_y);
   }
}

static void 
request_set_selection_notify(struct wl_listener *listener, void *data) 
{
   //say(DEBUG, "seat_request_set_selection_notify");
   struct simple_server *server = wl_container_of(listener, server, request_set_selection);
   struct wlr_seat_request_set_selection_event *event = data;
   wlr_seat_set_selection(server->seat, event->source, event->serial);
}

static void
request_set_primary_selection_notify(struct wl_listener *listener, void *data)
{
   struct simple_server *server = wl_container_of(listener, server, request_set_primary_selection);
   struct wlr_seat_request_set_primary_selection_event *event = data;
   wlr_seat_set_primary_selection(server->seat, event->source, event->serial);
}

//--- Input notify function ----------------------------------------------
static void 
input_destroy_notify(struct wl_listener *listener, void *data) 
{
   say(DEBUG, "input_destroy_notify");
   struct simple_input *input = wl_container_of(listener, input, destroy);
   if (input->type==INPUT_KEYBOARD) {
      wl_list_remove(&input->kb_modifiers.link);
      wl_list_remove(&input->kb_key.link);
   }
   wl_list_remove(&input->destroy.link);
   wl_list_remove(&input->link);
   free(input);
}

static void 
new_input_notify(struct wl_listener *listener, void *data) 
{
   //say(DEBUG, "new_input_notify");
   struct simple_server *server = wl_container_of(listener, server, new_input);
   struct wlr_input_device *device = data;

   struct simple_input *input = calloc(1, sizeof(struct simple_input));
   input->device = device;
   input->server = server;

   if(device->type == WLR_INPUT_DEVICE_POINTER) {
      say(DEBUG, "New Input: POINTER");
      input->type = INPUT_POINTER;
      wlr_cursor_attach_input_device(server->cursor, input->device);

   } else if (device->type == WLR_INPUT_DEVICE_KEYBOARD) {
      say(DEBUG, "New Input: KEYBOARD");
      input->type = INPUT_KEYBOARD;
      struct wlr_keyboard *kb = wlr_keyboard_from_input_device(device);
      input->keyboard = kb;

      struct xkb_rule_names rules = { 0 };
      struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
      struct xkb_keymap *keymap = xkb_map_new_from_names(context, &rules, 
            XKB_KEYMAP_COMPILE_NO_FLAGS);

      wlr_keyboard_set_keymap(kb, keymap);
      xkb_keymap_unref(keymap);
      xkb_context_unref(context);
      wlr_keyboard_set_repeat_info(kb, 25, 600);

      LISTEN(&kb->events.modifiers, &input->kb_modifiers, kb_modifiers_notify);
      LISTEN(&kb->events.key, &input->kb_key, kb_key_notify);
      
      wlr_seat_set_keyboard(server->seat, kb);
   } else {
      say(DEBUG, "New Input: SOMETHING ELSE");
      input->type = INPUT_MISC;
   }

   LISTEN(&device->events.destroy, &input->destroy, input_destroy_notify);
   wl_list_insert(&server->inputs, &input->link);

   uint32_t caps = 0;
   wl_list_for_each(input, &server->inputs, link) {
      switch (input->device->type){
         case WLR_INPUT_DEVICE_KEYBOARD:
            caps |= WL_SEAT_CAPABILITY_KEYBOARD;
            break;
         case WLR_INPUT_DEVICE_POINTER:
            caps |= WL_SEAT_CAPABILITY_POINTER;
            break;
         default:
            break;
      }
   }
   wlr_seat_set_capabilities(server->seat, caps);
}

//------------------------------------------------------------------------
void
relay_input_method_notify(struct wl_listener *listener, void* data)
{
   //
   say(DEBUG, "relay_input_method_notify");
}

void
relay_text_input_notify(struct wl_listener *listener, void* data)
{
   //
   say(DEBUG, "relay_text_input_notify");
}

//------------------------------------------------------------------------
void 
prepareServer(struct simple_server *server, struct wlr_session *session, int info_level) 
{
   say(INFO, "Preparing Wayland server");
   
   wlr_log_init(info_level, NULL);

   if(!(server->display = wl_display_create()))
      say(ERROR, "Unable to create Wayland display!");

   if(!(server->backend = wlr_backend_autocreate(server->display, &session)))
      say(ERROR, "Unable to create wlr_backend!");

   // create a scene graph used to lay out windows
   /* 
    * | layer      | type          | example  |
    * |------------|---------------|----------|
    * | LyrLock    | lock-manager  | swaylock |
    * | LyrOverlay | layer-shell   |          |
    * | LyrTop     | layer-shell   | waybar   |
    * | LyrClient  | normal client |          |
    * | LyrBottom  | layer-shell   |          |
    * | LyrBg      | layer-shell   | wbg      |
    */
   server->scene = wlr_scene_create();
   for(int i=0; i<NLayers; i++)
      server->layer_tree[i] = wlr_scene_tree_create(&server->scene->tree);

   // create renderer
   if(!(server->renderer = wlr_renderer_autocreate(server->backend)))
      say(ERROR, "Unable to create wlr_renderer");

   wlr_renderer_init_wl_display(server->renderer, server->display);
 
   // create an allocator
   if(!(server->allocator = wlr_allocator_autocreate(server->backend, server->renderer)))
      say(ERROR, "Unable to create wlr_allocator");

   // create compositor
   server->compositor = wlr_compositor_create(server->display, COMPOSITOR_VERSION, server->renderer);
   wlr_subcompositor_create(server->display);
   wlr_data_device_manager_create(server->display);
   
   wlr_export_dmabuf_manager_v1_create(server->display);
   wlr_screencopy_manager_v1_create(server->display);
   wlr_data_control_manager_v1_create(server->display);
   wlr_viewporter_create(server->display);
   wlr_single_pixel_buffer_manager_v1_create(server->display);
   wlr_primary_selection_v1_device_manager_create(server->display);
   wlr_fractional_scale_manager_v1_create(server->display, FRAC_SCALE_VERSION);
   
   // create an output layout, i.e. wlroots utility for working with an arrangement of 
   // screens in a physical layout
   server->output_layout = wlr_output_layout_create();
   LISTEN(&server->output_layout->events.change, &server->output_layout_change, output_layout_change_notify);

   wl_list_init(&server->outputs);   
   LISTEN(&server->backend->events.new_output, &server->new_output, new_output_notify);

   server->scene_output_layout = wlr_scene_attach_output_layout(server->scene, server->output_layout);

   wlr_xdg_output_manager_v1_create(server->display, server->output_layout);
   server->output_manager = wlr_output_manager_v1_create(server->display);
   LISTEN(&server->output_manager->events.apply, &server->output_manager_apply, output_manager_apply_notify);
   LISTEN(&server->output_manager->events.test, &server->output_manager_test, output_manager_test_notify);
   
   // set up seat and inputs
   server->seat = wlr_seat_create(server->display, "seat0");
   if(!server->seat)
      say(ERROR, "cannot allocate seat");

   wl_list_init(&server->inputs);
   LISTEN(&server->backend->events.new_input, &server->new_input, new_input_notify);

   LISTEN(&server->seat->events.request_set_cursor, &server->request_cursor, request_cursor_notify);
   LISTEN(&server->seat->events.request_set_selection, &server->request_set_selection, request_set_selection_notify);
   LISTEN(&server->seat->events.request_set_primary_selection, &server->request_set_primary_selection, request_set_primary_selection_notify);
   //LISTEN(&server->seat->events.request_set_drag, &server->request_set_drag, request_set_drag_notify);
   //LISTEN(&server->seat->events.start_drag, &server->start_drag, start_drag_notify);

   server->cursor = wlr_cursor_create();
   wlr_cursor_attach_output_layout(server->cursor, server->output_layout); 

   // create a cursor manager
   server->cursor_manager = wlr_xcursor_manager_create(NULL, 24);
   wlr_xcursor_manager_load(server->cursor_manager, 1);

   server->cursor_mode = CURSOR_PASSTHROUGH;
   LISTEN(&server->cursor->events.motion, &server->cursor_motion, cursor_motion_notify);
   LISTEN(&server->cursor->events.motion_absolute, &server->cursor_motion_abs, cursor_motion_abs_notify);
   LISTEN(&server->cursor->events.button, &server->cursor_button, cursor_button_notify);
   LISTEN(&server->cursor->events.axis, &server->cursor_axis, cursor_axis_notify);
   LISTEN(&server->cursor->events.frame, &server->cursor_frame, cursor_frame_notify);

   //input method init
   //wlr_foreign_toplevel_manager_v1_create(server->display);
   server->input_method = wlr_input_method_manager_v2_create(server->display);
   server->text_input = wlr_text_input_manager_v3_create(server->display);
   
   /*
   server->im_relay = calloc(1, sizeof(struct simple_input_method_relay));
   wl_list_init(&server->im_relay->text_inputs);

   LISTEN(&server->text_input->events.text_input, &server->im_relay->text_input_new, relay_text_input_notify);
   LISTEN(&server->input_method->events.input_method, &server->im_relay->input_method_new, relay_input_method_notify);
   */

   // set up Wayland shells, i.e. XDG and XWayland
   wl_list_init(&server->clients);
   wl_list_init(&server->focus_order);
   
   if(!(server->xdg_shell = wlr_xdg_shell_create(server->display, XDG_SHELL_VERSION)))
      say(ERROR, "unable to create XDG shell interface");
   LISTEN(&server->xdg_shell->events.new_surface, &server->xdg_new_surface, xdg_new_surface_notify);

   server->layer_shell = wlr_layer_shell_v1_create(server->display, LAYER_SHELL_VERSION);
   LISTEN(&server->layer_shell->events.new_surface, &server->layer_new_surface, layer_new_surface_notify);
   
   //unmanaged surfaces
   // ...

   // set up idle notifier and inhibit manager
   server->idle_notifier = wlr_idle_notifier_v1_create(server->display);
   server->idle_inhibit_manager = wlr_idle_inhibit_v1_create(server->display);
   LISTEN(&server->idle_inhibit_manager->events.new_inhibitor, &server->new_inhibitor, new_inhibitor_notify);

   // set up session lock manager
   server->session_lock_manager = wlr_session_lock_manager_v1_create(server->display);
   LISTEN(&server->session_lock_manager->events.new_lock, &server->new_lock_session_manager, new_lock_session_manager_notify);
   LISTEN(&server->session_lock_manager->events.destroy, &server->lock_session_manager_destroy, lock_session_manager_destroy_notify);

   // set up output power manager
   server->output_power_manager = wlr_output_power_manager_v1_create(server->display);
   LISTEN(&server->output_power_manager->events.set_mode, &server->output_pm_set_mode, output_pm_set_mode_notify);

   // set initial size - will be updated when output is changed
   server->locked_bg = wlr_scene_rect_create(server->layer_tree[LyrLock], 1, 1, (float [4]){0.1, 0.1, 0.1, 1.0});
   wlr_scene_node_set_enabled(&server->locked_bg->node, 0);

   // set initial background - will be updated when output is changed
   server->root_bg = wlr_scene_rect_create(server->layer_tree[LyrBg], 1, 1, server->config->background_colour);
   wlr_scene_node_set_enabled(&server->root_bg->node, 0);

   // Use decoration protocols to negotiate server-side decorations
   wlr_server_decoration_manager_set_default_mode(wlr_server_decoration_manager_create(server->display),
         WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
   server->xdg_decoration_manager = wlr_xdg_decoration_manager_v1_create(server->display);
   LISTEN(&server->xdg_decoration_manager->events.new_toplevel_decoration, &server->new_decoration, new_decoration_notify);

   struct wlr_presentation *presentation = wlr_presentation_create(server->display, server->backend);
   wlr_scene_set_presentation(server->scene, presentation);

   wl_global_create(server->display, &zdwl_ipc_manager_v2_interface, DWL_IPC_VERSION, NULL, ipc_manager_bind);

#if XWAYLAND
   if(!(server->xwayland = wlr_xwayland_create(server->display, server->compositor, true))) {
      say(WARNING, "unable to create xwayland server. Continuing without it");
      return;
   }

   LISTEN(&server->xwayland->events.new_surface, &server->xwl_new_surface, xwl_new_surface_notify);
   LISTEN(&server->xwayland->events.ready, &server->xwl_ready, xwl_ready_notify);
#endif
}

void 
startServer(struct simple_server *server) 
{
   say(INFO, "Starting Wayland server");

   const char* socket = wl_display_add_socket_auto(server->display);
   if(!socket){
      cleanupServer(server);
      say(ERROR, "Unable to add socket to Wayland display!");
   }

   if(!wlr_backend_start(server->backend)){
      cleanupServer(server);
      say(ERROR, "Unable to start WLR backend!");
   }
   
   setenv("WAYLAND_DISPLAY", socket, true);
   say(INFO, " -> Wayland server is running on WAYLAND_DISPLAY=%s ...", socket);

#if XWAYLAND
   if(setenv("DISPLAY", server->xwayland->display_name, true) < 0)
      say(WARNING, " -> Unable to set DISPLAY for xwayland");
   else 
      say(INFO, " -> XWayland is running on display %s", server->xwayland->display_name);
#endif

   // choose initial output based on cursor position
   server->cur_output = get_output_at(server, server->cursor->x, server->cursor->y);

   print_server_info(server);
}

void 
cleanupServer(struct simple_server *server) 
{
   say(INFO, "Cleaning up Wayland server");

#if XWAYLAND
   server->xwayland = NULL;
   wlr_xwayland_destroy(server->xwayland);
#endif

   wl_display_destroy_clients(server->display);
   wlr_xcursor_manager_destroy(server->cursor_manager);
   wlr_output_layout_destroy(server->output_layout);
   wl_display_destroy(server->display);
   // Destroy after the wayland display
   wlr_scene_node_destroy(&server->scene->tree.node);
}

