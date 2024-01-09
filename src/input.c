#include <linux/input-event-codes.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_input_method_v2.h>
#include <wlr/types/wlr_text_input_v3.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/edges.h>

#include "globals.h"
#include "layer.h"
#include "server.h"
#include "client.h"
#include "action.h"
#include "input.h"

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
   //int nsyms = xkb_state_key_get_syms(keyboard->keyboard->xkb_state, keycode, &syms);
   xkb_layout_index_t layout_index = xkb_state_key_get_layout(keyboard->keyboard->xkb_state, keycode);
   int nsyms = xkb_keymap_key_get_syms_by_level(keyboard->keyboard->keymap, 
      keycode, layout_index, 0, &syms);
   
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
   if(!client) return;

   client->geom.x = server->cursor->x - server->grab_x;
   client->geom.y = server->cursor->y - server->grab_y;

   set_client_geometry(client, client->geom);
}

static void 
process_cursor_resize(struct simple_server *server, uint32_t time) 
{
   struct simple_client *client = server->grabbed_client;
   if(!client) return;
   
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

void 
process_cursor_motion(struct simple_server *server, uint32_t time) 
{
   //say(DEBUG, "process_cursor_motion");

   // time is 0 in internal calls meant to restore point focus
   if(time>0){
      wlr_idle_notifier_v1_notify_activity(server->idle_notifier, server->seat);
   }

   // update drag icon's position
   wlr_scene_node_set_position(&server->drag_icon->node, server->cursor->x, server->cursor->y);

   if(server->cursor_mode == CURSOR_MOVE) {
      process_cursor_move(server, time);
      return;
   } else if(server->cursor_mode == CURSOR_RESIZE) {
      process_cursor_resize(server, time);
      return;
   } 

   // Otherwise, find the client under the pointer and send the event along
   double sx=0, sy=0;
   struct wlr_seat *wlr_seat = server->seat;
   struct wlr_surface *surface = NULL;
   struct simple_client *client = NULL;
   struct simple_layer_surface *lsurface = NULL;
   // We actually want the top level
   get_client_at(server, server->cursor->x, server->cursor->y, &client, &surface, &sx, &sy);

   if(server->cursor_mode==CURSOR_PRESSED && server->seat->drag){ 
      int ctype = get_client_from_surface(server->seat->pointer_state.focused_surface, &client, &lsurface);
      if(ctype!=-1){
         surface = server->seat->pointer_state.focused_surface;
         sx = server->cursor->x - (ctype==LAYER_SHELL_CLIENT ? lsurface->geom.x : client->geom.x);
         sy = server->cursor->y - (ctype==LAYER_SHELL_CLIENT ? lsurface->geom.y : client->geom.y);
      }
   }

   if(time>0 && client && server->config->sloppy_focus)
      focus_client(client, false);

   if(surface) {
      wlr_seat_pointer_notify_enter(wlr_seat, surface, sx, sy);
      wlr_seat_pointer_notify_motion(wlr_seat, time, sx, sy);
   } else {
      wlr_cursor_set_xcursor(server->cursor, server->cursor_manager, "left_ptr");
      wlr_seat_pointer_notify_clear_focus(wlr_seat);
   }
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
   //wlr_seat_pointer_notify_button(server->seat, event->time_msec, event->button, event->state);

   wlr_idle_notifier_v1_notify_activity(server->idle_notifier, server->seat);

   if(server->locked) return;

   double sx, sy;
   struct wlr_surface *surface = NULL;
   struct simple_client *client = NULL;
   int ctype = get_client_at(server, server->cursor->x, server->cursor->y, &client, &surface, &sx, &sy);

   struct mousemap *mousemap;
   switch (event->state) {
      case WLR_BUTTON_RELEASED:
         // button release
         server->cursor_mode = CURSOR_PASSTHROUGH;
         server->grabbed_client = NULL;
         wlr_seat_pointer_notify_button(server->seat, event->time_msec, event->button, event->state);
         return;
         break;
      case WLR_BUTTON_PRESSED:
         // button press
         server->cursor_mode = CURSOR_PRESSED;
         struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
         uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard);
         // press on desktop
         if(!client && ctype==-1) {
            say(DEBUG, "press on desktop");
            wl_list_for_each(mousemap, &server->config->mouse_bindings, link) {
               if(modifiers ^ mousemap->mask) continue;

               if(mousemap->context==CONTEXT_ROOT && event->button == mousemap->button){
                  mouse_function(NULL, mousemap, 0);
                  return;
               }
            }
            return;
         }
   
         //press on client 
         if(ctype!=LAYER_SHELL_CLIENT){
            focus_client(client, true);
            uint32_t resize_edges = get_resize_edges(client, server->cursor->x, server->cursor->y);
            wl_list_for_each(mousemap, &server->config->mouse_bindings, link) {
               if(modifiers ^ mousemap->mask) continue;

               if(mousemap->context==CONTEXT_CLIENT && event->button == mousemap->button){
                  mouse_function(client, mousemap, resize_edges);
                  return;
               }
            }
         }
         break;
   } // switch
   
   //focus_client(client, true);
   wlr_seat_pointer_notify_button(server->seat, event->time_msec, event->button, event->state);
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

static void
destroy_drag_icon_notify(struct wl_listener *listener, void *data)
{
   struct simple_server *server = wl_container_of(listener, server, destroy_drag_icon);

   focus_client(get_top_client_from_output(server->cur_output, false), true);
}

static void
request_start_drag_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "request_start_drag_notify");
   struct simple_server *server = wl_container_of(listener, server, request_start_drag);
   struct wlr_seat_request_start_drag_event *event = data;

   if(wlr_seat_validate_pointer_grab_serial(server->seat, event->origin, event->serial))
      wlr_seat_start_pointer_drag(server->seat, event->drag, event->serial);
   else
      wlr_data_source_destroy(event->drag->source);
}

static void
start_drag_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "start_drag_notify");
   struct simple_server *server = wl_container_of(listener, server, start_drag);
   struct wlr_drag *drag = data;
   if(!drag->icon) return;

   drag->icon->data = &wlr_scene_drag_icon_create(server->drag_icon, drag->icon)->node;
   LISTEN(&drag->icon->events.destroy, &server->destroy_drag_icon, destroy_drag_icon_notify);
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
input_init(struct simple_server *server)
{
   wl_list_init(&server->inputs);
   LISTEN(&server->backend->events.new_input, &server->new_input, new_input_notify);

   LISTEN(&server->seat->events.request_set_cursor, &server->request_cursor, request_cursor_notify);
   LISTEN(&server->seat->events.request_set_selection, &server->request_set_selection, request_set_selection_notify);
   LISTEN(&server->seat->events.request_set_primary_selection, &server->request_set_primary_selection, request_set_primary_selection_notify);

   server->drag_icon = wlr_scene_tree_create(&server->scene->tree);
   wlr_scene_node_place_below(&server->drag_icon->node, &server->layer_tree[LyrLock]->node);
   LISTEN(&server->seat->events.request_start_drag, &server->request_start_drag, request_start_drag_notify);
   LISTEN(&server->seat->events.start_drag, &server->start_drag, start_drag_notify);

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
   server->input_method = wlr_input_method_manager_v2_create(server->display);
   server->text_input = wlr_text_input_manager_v3_create(server->display);
   
   /*
   server->im_relay = calloc(1, sizeof(struct simple_input_method_relay));
   wl_list_init(&server->im_relay->text_inputs);

   LISTEN(&server->text_input->events.text_input, &server->im_relay->text_input_new, relay_text_input_notify);
   LISTEN(&server->input_method->events.input_method, &server->im_relay->input_method_new, relay_input_method_notify);
   */
}
