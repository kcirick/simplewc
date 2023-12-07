#include <linux/input-event-codes.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/xwayland.h>
#include <xkbcommon/xkbcommon.h>

#include "globals.h"
#include "server.h"
#include "seat.h"
#include "action.h"

/*
      case XKB_KEY_F1:
         if(wl_list_length(&server->views) < 2) break;
         
         struct simple_view *current_view = wl_container_of(server->views.next, current_view, link);
         struct simple_view *next_view = wl_container_of(current_view->link.next, next_view, link);
         focus_view(next_view);
         
         wl_list_remove(&current_view->link);
         wl_list_insert(server->views.prev, &current_view->link);
*/

static void kb_modifiers_notify(struct wl_listener *listener, void *data) {
   struct simple_input *keyboard = wl_container_of(listener, keyboard, kb_modifiers);

   wlr_seat_set_keyboard(keyboard->seat->seat, keyboard->keyboard);
   wlr_seat_keyboard_notify_modifiers(keyboard->seat->seat, &keyboard->keyboard->modifiers);
}

static void kb_key_notify(struct wl_listener *listener, void *data) {
   struct simple_input *keyboard = wl_container_of(listener, keyboard, kb_key);
   struct simple_server *server = keyboard->seat->server;
   struct wlr_keyboard_key_event *event = data;
   struct wlr_seat *wlr_seat = keyboard->seat->seat;

   uint32_t keycode = event->keycode + 8;
   const xkb_keysym_t *syms;
   int nsyms = xkb_state_key_get_syms(keyboard->keyboard->xkb_state, keycode, &syms);
   
   bool handled = false;
   uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->keyboard);

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
      wlr_seat_set_keyboard(wlr_seat, keyboard->keyboard);
      wlr_seat_keyboard_notify_key(wlr_seat, event->time_msec, event->keycode, event->state);
   }
}

//--- Pointer events -----------------------------------------------------
static uint32_t get_resize_edges(struct simple_view *view, double x, double y) {
   uint32_t edges = 0;

   struct wlr_box box = view->current;
   edges |= x < (box.x + box.width/2)  ? WLR_EDGE_LEFT : WLR_EDGE_RIGHT;
   edges |= y < (box.y + box.height/2) ? WLR_EDGE_TOP : WLR_EDGE_BOTTOM;
   return edges;
}

static void process_cursor_move(struct simple_server *server, uint32_t time) {
   struct simple_view *view = server->grabbed_view;
   view->current.x = server->seat->cursor->x - server->grab_x;
   view->current.y = server->seat->cursor->y - server->grab_y;
   wlr_scene_node_set_position(&view->scene_tree->node, view->current.x, view->current.y);
}

static void process_cursor_resize(struct simple_server *server, uint32_t time) {
   struct simple_view *view = server->grabbed_view;
   
   double delta_x = server->seat->cursor->x - server->grab_x;
   double delta_y = server->seat->cursor->y - server->grab_y;
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

   view->current.x = new_left;
   view->current.y = new_top;
   view->current.width = new_right - new_left;
   view->current.height = new_bottom - new_top;

   wlr_scene_node_set_position(&view->scene_tree->node, view->current.x, view->current.y);
   if(view->type==XDG_SHELL_VIEW){
      wlr_xdg_toplevel_set_size(view->xdg_toplevel, view->current.width, view->current.height);
#if XWAYLAND
   } else {
      wlr_xwayland_surface_configure(view->xwayland_surface,
            view->current.x, view->current.y, view->current.width, view->current.height);
#endif
   }

}

static void process_cursor_motion(struct simple_server *server, uint32_t time) {
   //say(DEBUG, "process_cursor_motion");
   if(server->cmode == CURSOR_MOVE) {
      process_cursor_move(server, time);
      return;
   } else if(server->cmode == CURSOR_RESIZE) {
      process_cursor_resize(server, time);
      return;
   } 

   // Otherwise, find teh view under the pointer and send the event along
   double sx, sy;
   struct wlr_seat *wlr_seat = server->seat->seat;
   struct wlr_surface *surface = NULL;
   struct simple_view *view = desktop_view_at(server, server->seat->cursor->x, server->seat->cursor->y, &surface, &sx, &sy);
   
   char *cname = NULL;
   if(!view) {
      cname = "left_ptr";
   } else {
      //get_resize_edges();
      //switch resize_edges:
   }
   if(cname)
      wlr_xcursor_manager_set_cursor_image(server->seat->cursor_manager, cname, server->seat->cursor);

   if(surface) {
      bool focus_changed = wlr_seat->pointer_state.focused_surface != surface;

      wlr_seat_pointer_notify_enter(wlr_seat, surface, sx, sy);
      if(!focus_changed)
         wlr_seat_pointer_notify_motion(wlr_seat, time, sx, sy);
   } else
      wlr_seat_pointer_clear_focus(wlr_seat);
}

static void cursor_motion_notify(struct wl_listener *listener, void *data) {
   //say(DEBUG, "cursor_motion_notify");
   struct simple_seat *seat = wl_container_of(listener, seat, cursor_motion);
   struct wlr_pointer_motion_event *event = data;

   wlr_cursor_move(seat->cursor, &event->pointer->base, event->delta_x, event->delta_y);
   process_cursor_motion(seat->server, event->time_msec);
}

static void cursor_motion_abs_notify(struct wl_listener *listener, void *data) {
  // say(DEBUG, "cursor_motion_abs_notify");
   struct simple_seat *seat = wl_container_of(listener, seat, cursor_motion_abs);
   struct wlr_pointer_motion_absolute_event *event = data;

   wlr_cursor_warp_absolute(seat->cursor, &event->pointer->base, event->x, event->y);
   process_cursor_motion(seat->server, event->time_msec);
}

static void cursor_button_notify(struct wl_listener *listener, void *data) {
   say(DEBUG, "cursor_button_notify");
   struct simple_seat *seat = wl_container_of(listener, seat, cursor_button);
   struct simple_server *server = seat->server;
   struct wlr_pointer_button_event *event = data;
   
   // Notify the client with pointer focus that a button press has occurred
   wlr_seat_pointer_notify_button(seat->seat, event->time_msec, event->button, event->state);

   double sx, sy;
   struct wlr_surface *surface = NULL;
   //uint32_t resize_edges;
   struct simple_view *view = desktop_view_at(server, server->seat->cursor->x, server->seat->cursor->y, &surface, &sx, &sy);

   // button release
   if(event->state == WLR_BUTTON_RELEASED) {
      server->cmode = CURSOR_PASSTHROUGH;
      server->grabbed_view = NULL;
      return;
   }
   
   // press on desktop
   if(!view) {
      say(INFO, "press on desktop");
      return;
   }
   
   //press on view
   struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat->seat);
   uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard);
   if((modifiers & WLR_MODIFIER_ALT) && (event->button == BTN_LEFT) ) {
      begin_interactive(view, CURSOR_MOVE, 0);
      return;
   } else if ((modifiers & WLR_MODIFIER_ALT) && (event->button == BTN_RIGHT) ) {
      uint32_t resize_edges = get_resize_edges(view, server->seat->cursor->x, server->seat->cursor->y);
      begin_interactive(view, CURSOR_RESIZE, resize_edges);
      return;
   }
   
   focus_view(view, surface);
}

static void cursor_axis_notify(struct wl_listener *listener, void *data) {
   //say(DEBUG, "cursor_axis_notify");
   struct simple_seat *seat = wl_container_of(listener, seat, cursor_axis);
   struct wlr_pointer_axis_event *event = data;

   wlr_seat_pointer_notify_axis(seat->seat, event->time_msec, event->orientation, event->delta, event->delta_discrete, event->source);
}

static void cursor_frame_notify(struct wl_listener *listener, void *data) {
   //say(DEBUG, "cursor_frame_notify");
   struct simple_seat *seat = wl_container_of(listener, seat, cursor_frame);

   wlr_seat_pointer_notify_frame(seat->seat);
}

static void seat_request_cursor_notify(struct wl_listener *listener, void *data) {
   //say(DEBUG, "seat_request_cursor_notify");
   struct simple_seat *seat = wl_container_of(listener, seat, request_cursor);
   struct wlr_seat_pointer_request_set_cursor_event *event = data;
   struct wlr_seat_client *focused_client = seat->seat->pointer_state.focused_client;
   
   if(focused_client == event->seat_client) {
      wlr_cursor_set_surface(seat->cursor, event->surface, event->hotspot_x, event->hotspot_y);
   }
}

static void seat_request_set_selection_notify(struct wl_listener *listener, void *data) {
   //say(DEBUG, "seat_request_set_selection_notify");
   struct simple_seat *seat = wl_container_of(listener, seat, request_set_selection);
   struct wlr_seat_request_set_selection_event *event = data;
   wlr_seat_set_selection(seat->seat, event->source, event->serial);
}

static void input_destroy_notify(struct wl_listener *listener, void *data) {
   say(DEBUG, "input_destroy_notify");
   struct simple_input *input = wl_container_of(listener, input, destroy);
   //wl_list_remove(&input->kb_modifiers.link);
   //wl_list_remove(&input->kb_key.link);
   wl_list_remove(&input->destroy.link);
   wl_list_remove(&input->link);
   free(input);
}

static void new_input_notify(struct wl_listener *listener, void *data) {
   //say(DEBUG, "new_input_notify");
   struct simple_seat *seat = wl_container_of(listener, seat, new_input);
   struct wlr_input_device *device = data;

   struct simple_input *input = calloc(1, sizeof(struct simple_input));
   input->device = device;
   input->seat = seat;

   if(device->type == WLR_INPUT_DEVICE_POINTER) {
      say(DEBUG, "New Input: POINTER");
      wlr_cursor_attach_input_device(seat->cursor, input->device);

   } else if (device->type == WLR_INPUT_DEVICE_KEYBOARD) {
      say(DEBUG, "New Input: KEYBOARD");
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

      input->kb_modifiers.notify = kb_modifiers_notify;
      wl_signal_add(&kb->events.modifiers, &input->kb_modifiers);
      input->kb_key.notify = kb_key_notify;
      wl_signal_add(&kb->events.key, &input->kb_key);
      
      wlr_seat_set_keyboard(seat->seat, kb);
   } else 
      say(DEBUG, "New Input: SOMETHING ELSE");

   input->destroy.notify = input_destroy_notify;
   wl_signal_add(&device->events.destroy, &input->destroy);
   wl_list_insert(&seat->inputs, &input->link);

   uint32_t caps = 0;
   wl_list_for_each(input, &seat->inputs, link) {
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
   wlr_seat_set_capabilities(seat->seat, caps);
}

//------------------------------------------------------------------------
void initializeCursor(struct simple_server *server, struct simple_seat *seat) {

   seat->cursor = wlr_cursor_create();
   wlr_cursor_attach_output_layout(seat->cursor, server->output_layout); 

   // create a cursor manager
   seat->cursor_manager = wlr_xcursor_manager_create(NULL, 24);
   wlr_xcursor_manager_load(seat->cursor_manager, 1);

   server->cmode = CURSOR_PASSTHROUGH;
   seat->cursor_motion.notify = cursor_motion_notify;
   wl_signal_add(&seat->cursor->events.motion, &seat->cursor_motion);
   seat->cursor_motion_abs.notify = cursor_motion_abs_notify;
   wl_signal_add(&seat->cursor->events.motion_absolute, &seat->cursor_motion_abs);
   seat->cursor_button.notify = cursor_button_notify;
   wl_signal_add(&seat->cursor->events.button, &seat->cursor_button);
   seat->cursor_axis.notify = cursor_axis_notify;
   wl_signal_add(&seat->cursor->events.axis, &seat->cursor_axis);
   seat->cursor_frame.notify = cursor_frame_notify;
   wl_signal_add(&seat->cursor->events.frame, &seat->cursor_frame);
}

void initializeSeat(struct simple_server *server) {
   
   server->seat = calloc(1, sizeof(struct simple_seat));

   struct simple_seat *seat = server->seat;
   seat->server = server;

   seat->seat = wlr_seat_create(server->display, "seat0");
   if(!seat->seat)
      say(ERROR, "cannot allocate seat");

   wl_list_init(&seat->inputs);
   seat->new_input.notify = new_input_notify;
   wl_signal_add(&server->backend->events.new_input, &seat->new_input);

   seat->request_cursor.notify = seat_request_cursor_notify;
   wl_signal_add(&seat->seat->events.request_set_cursor, &seat->request_cursor);
   seat->request_set_selection.notify = seat_request_set_selection_notify;
   wl_signal_add(&seat->seat->events.request_set_selection, &seat->request_set_selection);

   initializeCursor(server, seat);
}

void seat_focus_surface(struct simple_seat *seat, struct wlr_surface *surface) {
   if(!surface) {
      wlr_seat_keyboard_notify_clear_focus(seat->seat);
      return;
   }
   struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat->seat);
      
   wlr_seat_keyboard_notify_enter(seat->seat, surface, keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
}
