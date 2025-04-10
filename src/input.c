#include <string.h>
#include <linux/input-event-codes.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_input_method_v2.h>
#include <wlr/types/wlr_text_input_v3.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>

#include "globals.h"
#include "layer.h"
#include "server.h"
#include "output.h"
#include "client.h"
#include "action.h"
#include "input.h"

//--- Input functions ----------------------------------------------------
void 
input_focus_surface(struct wlr_surface *surface) 
{
   if(!surface) {
      wlr_seat_keyboard_notify_clear_focus(g_server->seat);
      return;
   }

   struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(g_server->seat);
      
   wlr_seat_keyboard_notify_enter(g_server->seat, surface, keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
}

//--- Keyboard events ----------------------------------------------------
static void 
kb_modifiers_notify(struct wl_listener *listener, void *data) 
{
   struct simple_input *keyboard = wl_container_of(listener, keyboard, kb_modifiers);
   struct wlr_keyboard* wlr_kb = keyboard->keyboard;

   wlr_idle_notifier_v1_notify_activity(g_server->idle_notifier, g_server->seat);

   if(g_server->grabbed_client) {
      // we are still cycling through the windows
      xkb_mod_index_t i;
      bool mod_pressed = false;
      for(i=0; i<xkb_keymap_num_mods(wlr_kb->keymap); i++){
         if(xkb_state_mod_index_is_active(wlr_kb->xkb_state, i, XKB_STATE_MODS_DEPRESSED))
            mod_pressed = true;
      }
      if(!mod_pressed) {
         struct simple_client* client = g_server->grabbed_client;
         if(g_server->grabbed_client_outline){
            wlr_scene_node_destroy(&g_server->grabbed_client_outline->tree->node);
            g_server->grabbed_client_outline=NULL;
         }

         // change stacking order and focus client 
         wl_list_remove(&client->link);
         wl_list_insert(&g_server->clients, &client->link);
         g_server->grabbed_client=NULL;
         focus_client(client, true);
         arrange_output(g_server->cur_output);
      }
   }

   wlr_seat_keyboard_notify_modifiers(g_server->seat, &keyboard->keyboard->modifiers);
}

static void 
kb_key_notify(struct wl_listener *listener, void *data) 
{
   struct simple_input *keyboard = wl_container_of(listener, keyboard, kb_key);
   struct wlr_keyboard_key_event *event = data;

   uint32_t keycode = event->keycode + 8;
   const xkb_keysym_t *syms;
   //int nsyms = xkb_state_key_get_syms(keyboard->keyboard->xkb_state, keycode, &syms);
   xkb_layout_index_t layout_index = xkb_state_key_get_layout(keyboard->keyboard->xkb_state, keycode);
   int nsyms = xkb_keymap_key_get_syms_by_level(keyboard->keyboard->keymap, 
      keycode, layout_index, 0, &syms);
   
   bool handled = false;
   uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->keyboard);

   wlr_idle_notifier_v1_notify_activity(g_server->idle_notifier, g_server->seat);

   if(event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
      for(int i=0; i<nsyms; i++){
         struct keymap *keymap;
         wl_list_for_each(keymap, &g_config->key_bindings, link) {
            if (modifiers ^ keymap->mask) continue;

            if (syms[i] == keymap->keysym){
               key_function(keymap);
               handled=true;
            }
         }
      }
   }

   if(event->state == WL_KEYBOARD_KEY_STATE_RELEASED) {
      if(g_server->seat->keyboard_state.focused_surface){
         wlr_seat_set_keyboard(g_server->seat, keyboard->keyboard);
         wlr_seat_keyboard_notify_key(g_server->seat, event->time_msec, event->keycode, event->state);
         handled=true; 
      }
   }

   if(!handled && event->state != WL_KEYBOARD_KEY_STATE_RELEASED) {
      wlr_seat_set_keyboard(g_server->seat, keyboard->keyboard);
      wlr_seat_keyboard_notify_key(g_server->seat, event->time_msec, event->keycode, event->state);
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
process_cursor_move(uint32_t time) 
{
   struct simple_client *client = g_server->grabbed_client;
   if(!client) return;

   client->geom.x = g_server->cursor->x - g_server->grab_x;
   client->geom.y = g_server->cursor->y - g_server->grab_y;

   set_client_geometry(client);
}

static void 
process_cursor_resize(uint32_t time) 
{
   struct simple_client *client = g_server->grabbed_client;
   if(!client) return;
   
   double delta_x = g_server->cursor->x - g_server->grab_x;
   double delta_y = g_server->cursor->y - g_server->grab_y;
   int new_left = g_server->grab_box.x;
   int new_right = g_server->grab_box.x + g_server->grab_box.width;
   int new_top = g_server->grab_box.y;
   int new_bottom = g_server->grab_box.y + g_server->grab_box.height;
   
   if (g_server->resize_edges & WLR_EDGE_TOP) {
      new_top += delta_y;
      if(new_top >= new_bottom)
         new_top = new_bottom - 1;
   } else if (g_server->resize_edges & WLR_EDGE_BOTTOM) {
      new_bottom += delta_y;
      if(new_bottom <= new_top)
         new_bottom = new_top + 1;
   }
   
   if (g_server->resize_edges & WLR_EDGE_LEFT) {
      new_left += delta_x;
      if(new_left >= new_right)
         new_left = new_right - 1;
   } else if (g_server->resize_edges & WLR_EDGE_RIGHT) {
      new_right += delta_x;
      if(new_right <= new_left)
         new_right = new_left + 1;
   }

   client->geom.x = new_left;
   client->geom.y = new_top;
   client->geom.width = new_right - new_left;
   client->geom.height = new_bottom - new_top;

   if(client->type==XDG_SHELL_CLIENT)
      wlr_xdg_toplevel_set_bounds(client->xdg_surface->toplevel, client->geom.width, client->geom.height);
   
   set_client_geometry(client);
}

static void 
process_cursor_motion(uint32_t time, struct wlr_input_device *device, double dx, double dy,
      double dx_unaccel, double dy_unaccel) 
{
   //say(DEBUG, "process_cursor_motion");

   // time is 0 in internal calls meant to restore point focus
   if(time>0){
      wlr_idle_notifier_v1_notify_activity(g_server->idle_notifier, g_server->seat);

      wlr_relative_pointer_manager_v1_send_relative_motion(
            g_server->relative_pointer_manager, g_server->seat, (uint64_t)time*1000,
            dx, dy, dx_unaccel, dy_unaccel);
   }

   // update drag icon's position
   wlr_scene_node_set_position(&g_server->drag_icon->node, g_server->cursor->x, g_server->cursor->y);

   if(g_server->cursor_mode == CURSOR_MOVE) {
      process_cursor_move(time);
      return;
   } else if(g_server->cursor_mode == CURSOR_RESIZE) {
      process_cursor_resize(time);
      return;
   } 

   // Otherwise, find the client under the pointer and send the event along
   double sx=0, sy=0;
   struct wlr_seat *wlr_seat = g_server->seat;
   struct wlr_surface *surface = NULL;
   struct simple_client *client = NULL, *focused_client = NULL;
   struct simple_layer_surface *lsurface = NULL;
   // We actually want the top level
   int ctype = get_client_at(g_server->cursor->x, g_server->cursor->y, &client, &surface, &sx, &sy);

   int ctype_focused = get_client_from_surface(g_server->seat->keyboard_state.focused_surface, &focused_client, &lsurface);
   if(g_server->cursor_mode==CURSOR_PRESSED && !g_server->seat->drag){ 
      if(ctype_focused!=-1){
         surface = g_server->seat->pointer_state.focused_surface;
         sx = g_server->cursor->x - (ctype_focused==LAYER_SHELL_CLIENT ? lsurface->geom.x : focused_client->geom.x);
         sy = g_server->cursor->y - (ctype_focused==LAYER_SHELL_CLIENT ? lsurface->geom.y : focused_client->geom.y);
      }
   }

   /*
   if(time>0 && client && ctype!=LAYER_SHELL_CLIENT && ctype_focused!=LAYER_SHELL_CLIENT 
      && client != focused_client && g_config->sloppy_focus && !g_server->seat->drag)
      focus_client(client, false);
   */
   if(time>0 && client && ctype!=LAYER_SHELL_CLIENT && ctype_focused!=LAYER_SHELL_CLIENT 
      && client != focused_client && g_config->focus_type>0 && !g_server->seat->drag)
      focus_client(client, g_config->focus_type==RAISE);

   if(surface) {
      wlr_seat_pointer_notify_enter(wlr_seat, surface, sx, sy);
      wlr_seat_pointer_notify_motion(wlr_seat, time, sx, sy);
   } else {
      wlr_cursor_set_xcursor(g_server->cursor, g_server->cursor_manager, "left_ptr");
      wlr_seat_pointer_notify_clear_focus(wlr_seat);
   }
} 

//--- cursor notify functions --------------------------------------------
static void 
cursor_motion_notify(struct wl_listener *listener, void *data) 
{
   //say(DEBUG, "cursor_motion_notify");
   struct wlr_pointer_motion_event *event = data;

   wlr_cursor_move(g_server->cursor, &event->pointer->base, event->delta_x, event->delta_y);
   process_cursor_motion(event->time_msec, &event->pointer->base, event->delta_x, event->delta_y, 
         event->unaccel_dx, event->unaccel_dy);
}

static void 
cursor_motion_abs_notify(struct wl_listener *listener, void *data) 
{
   //say(DEBUG, "cursor_motion_abs_notify");
   struct wlr_pointer_motion_absolute_event *event = data;
   double lx, ly, dx, dy;

   wlr_cursor_warp_absolute(g_server->cursor, &event->pointer->base, event->x, event->y);
   wlr_cursor_absolute_to_layout_coords(g_server->cursor, &event->pointer->base, event->x, event->y, &lx, &ly);
   dx = lx - g_server->cursor->x;
   dy = ly - g_server->cursor->y;
   process_cursor_motion(event->time_msec, &event->pointer->base, dx, dy, dx, dy);
}

static void 
cursor_button_notify(struct wl_listener *listener, void *data) 
{
   say(DEBUG, "cursor_button_notify");
   struct wlr_pointer_button_event *event = data;

   wlr_idle_notifier_v1_notify_activity(g_server->idle_notifier, g_server->seat);

   if(g_server->locked) return;

   double sx, sy;
   struct wlr_surface *surface = NULL;
   struct simple_client *client = NULL;
   int ctype = get_client_at(g_server->cursor->x, g_server->cursor->y, &client, &surface, &sx, &sy);

   struct mousemap *mousemap;
   switch (event->state) {
      case WLR_BUTTON_RELEASED:
         // button release
         wlr_cursor_set_xcursor(g_server->cursor, g_server->cursor_manager, "left_ptr");
         g_server->cursor_mode = CURSOR_NORMAL;
         g_server->grabbed_client = NULL;
         //wlr_seat_pointer_notify_button(g_server->seat, event->time_msec, event->button, event->state);
         //return;
         break;
      case WLR_BUTTON_PRESSED:
         // button press
         g_server->cursor_mode = CURSOR_PRESSED;
         struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(g_server->seat);
         uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard);
         // press on desktop
         if(!client && ctype==-1) {
            say(DEBUG, "press on desktop");
            wl_list_for_each(mousemap, &g_config->mouse_bindings, link) {
               if(modifiers ^ mousemap->mask) continue;

               if(mousemap->context==CONTEXT_ROOT && event->button == mousemap->button){
                  mouse_function(NULL, mousemap, 0);
                  return;
               }
            }
         } else if(ctype!=LAYER_SHELL_CLIENT) { //press on client
            focus_client(client, true);
            uint32_t resize_edges = get_resize_edges(client, g_server->cursor->x, g_server->cursor->y);
            wl_list_for_each(mousemap, &g_config->mouse_bindings, link) {
               if(modifiers ^ mousemap->mask) continue;

               if(mousemap->context==CONTEXT_CLIENT && event->button == mousemap->button){
                  mouse_function(client, mousemap, resize_edges);
                  return;
               }
            }
         } else {
            say(DEBUG, "Layer Shell input");
            input_focus_surface(surface);
         }
         break;
   } // switch
   
   // Notify the client with pointer focus that a button press has occurred
   wlr_seat_pointer_notify_button(g_server->seat, event->time_msec, event->button, event->state);
}

static void 
cursor_axis_notify(struct wl_listener *listener, void *data) 
{
   //say(DEBUG, "cursor_axis_notify");
   struct wlr_pointer_axis_event *event = data;

   wlr_idle_notifier_v1_notify_activity(g_server->idle_notifier, g_server->seat);
   wlr_seat_pointer_notify_axis(g_server->seat, event->time_msec, event->orientation, event->delta, event->delta_discrete, event->source, event->relative_direction);
}

static void 
cursor_frame_notify(struct wl_listener *listener, void *data) 
{
   //say(DEBUG, "cursor_frame_notify");

   wlr_seat_pointer_notify_frame(g_server->seat);
}

static void 
request_cursor_notify(struct wl_listener *listener, void *data) 
{
   //say(DEBUG, "seat_request_cursor_notify");
   struct wlr_seat_pointer_request_set_cursor_event *event = data;
   struct wlr_seat_client *focused_client = g_server->seat->pointer_state.focused_client;
   
   if(focused_client == event->seat_client) {
      wlr_cursor_set_surface(g_server->cursor, event->surface, event->hotspot_x, event->hotspot_y);
   }
}

static void 
request_set_selection_notify(struct wl_listener *listener, void *data) 
{
   //say(DEBUG, "seat_request_set_selection_notify");
   struct wlr_seat_request_set_selection_event *event = data;

   wlr_seat_set_selection(g_server->seat, event->source, event->serial);
}

static void
request_set_primary_selection_notify(struct wl_listener *listener, void *data)
{
   struct wlr_seat_request_set_primary_selection_event *event = data;

   wlr_seat_set_primary_selection(g_server->seat, event->source, event->serial);
}

static void
destroy_drag_icon_notify(struct wl_listener *listener, void *data)
{
   focus_client(get_top_client_from_output(g_server->cur_output, false), true);
}

static void
request_start_drag_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "request_start_drag_notify");
   struct wlr_seat_request_start_drag_event *event = data;

   if(wlr_seat_validate_pointer_grab_serial(g_server->seat, event->origin, event->serial))
      wlr_seat_start_pointer_drag(g_server->seat, event->drag, event->serial);
   else
      wlr_data_source_destroy(event->drag->source);
}

static void
start_drag_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "start_drag_notify");
   struct wlr_drag *drag = data;
   if(!drag->icon) return;

   drag->icon->data = &wlr_scene_drag_icon_create(g_server->drag_icon, drag->icon)->node;
   LISTEN(&drag->icon->events.destroy, &g_server->destroy_drag_icon, destroy_drag_icon_notify);
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
   struct wlr_input_device *device = data;

   struct simple_input *input = calloc(1, sizeof(struct simple_input));
   input->device = device;

   if(device->type == WLR_INPUT_DEVICE_POINTER) {
      say(DEBUG, "New Input: POINTER");
      input->type = INPUT_POINTER;
      wlr_cursor_attach_input_device(g_server->cursor, input->device);

      if(wlr_input_device_is_libinput(device)){
         struct libinput_device *libinput_device = wlr_libinput_get_device_handle(device);
         if(libinput_device_config_tap_get_finger_count(libinput_device) > 0 && g_config->touchpad_tap_click) {
            // touchpad - enable tap click
            libinput_device_config_tap_set_enabled(libinput_device, true);
         }
      }

   } else if (device->type == WLR_INPUT_DEVICE_KEYBOARD) {
      say(DEBUG, "New Input: KEYBOARD");
      input->type = INPUT_KEYBOARD;
      struct wlr_keyboard *kb = wlr_keyboard_from_input_device(device);
      input->keyboard = kb;

      struct xkb_rule_names rules = { 0 };
      if(g_config->xkb_layout[0] != '\0')
         rules.layout = g_config->xkb_layout;
      if(g_config->xkb_options[0] != '\0')
         rules.options = g_config->xkb_options;

      struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
      struct xkb_keymap *keymap = xkb_map_new_from_names(context, &rules, 
            XKB_KEYMAP_COMPILE_NO_FLAGS);

      wlr_keyboard_set_keymap(kb, keymap);
      xkb_keymap_unref(keymap);
      xkb_context_unref(context);
      wlr_keyboard_set_repeat_info(kb, 25, 600);

      LISTEN(&kb->events.modifiers, &input->kb_modifiers, kb_modifiers_notify);
      LISTEN(&kb->events.key, &input->kb_key, kb_key_notify);
      
      wlr_seat_set_keyboard(g_server->seat, kb);
   } else {
      say(DEBUG, "New Input: SOMETHING ELSE");
      input->type = INPUT_MISC;
   }

   LISTEN(&device->events.destroy, &input->destroy, input_destroy_notify);
   wl_list_insert(&g_server->inputs, &input->link);

   uint32_t caps = 0;
   wl_list_for_each(input, &g_server->inputs, link) {
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
   wlr_seat_set_capabilities(g_server->seat, caps);
}

//------------------------------------------------------------------------
void
input_init()
{
   wl_list_init(&g_server->inputs);
   LISTEN(&g_server->backend->events.new_input, &g_server->new_input, new_input_notify);

   LISTEN(&g_server->seat->events.request_set_cursor, &g_server->request_cursor, request_cursor_notify);
   LISTEN(&g_server->seat->events.request_set_selection, &g_server->request_set_selection, request_set_selection_notify);
   LISTEN(&g_server->seat->events.request_set_primary_selection, &g_server->request_set_primary_selection, request_set_primary_selection_notify);

   g_server->drag_icon = wlr_scene_tree_create(&g_server->scene->tree);
   wlr_scene_node_place_below(&g_server->drag_icon->node, &g_server->layer_tree[LyrLock]->node);
   LISTEN(&g_server->seat->events.request_start_drag, &g_server->request_start_drag, request_start_drag_notify);
   LISTEN(&g_server->seat->events.start_drag, &g_server->start_drag, start_drag_notify);

   g_server->cursor = wlr_cursor_create();
   wlr_cursor_attach_output_layout(g_server->cursor, g_server->output_layout); 

   // create a cursor manager
   g_server->cursor_manager = wlr_xcursor_manager_create(NULL, 24);
   wlr_xcursor_manager_load(g_server->cursor_manager, 1);

   g_server->relative_pointer_manager = wlr_relative_pointer_manager_v1_create(g_server->display);

   g_server->cursor_mode = CURSOR_NORMAL;
   LISTEN(&g_server->cursor->events.motion, &g_server->cursor_motion, cursor_motion_notify);
   LISTEN(&g_server->cursor->events.motion_absolute, &g_server->cursor_motion_abs, cursor_motion_abs_notify);
   LISTEN(&g_server->cursor->events.button, &g_server->cursor_button, cursor_button_notify);
   LISTEN(&g_server->cursor->events.axis, &g_server->cursor_axis, cursor_axis_notify);
   LISTEN(&g_server->cursor->events.frame, &g_server->cursor_frame, cursor_frame_notify);

   //input method init
   wlr_input_method_manager_v2_create(g_server->display);
   wlr_text_input_manager_v3_create(g_server->display);
}
