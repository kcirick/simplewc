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
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_tablet_v2.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>
#include <wlr/util/region.h>

#include "globals.h"
#include "layer.h"
#include "server.h"
#include "output.h"
#include "client.h"
#include "action.h"
#include "input.h"


//--- Pointer Constraints ------------------------------------------------
void
cursor_constrain(struct wlr_pointer_constraint_v1 *constraint)
{
   if(g_server->active_constraint == constraint)
      return;

   if(g_server->active_constraint)
      wlr_pointer_constraint_v1_send_deactivated(g_server->active_constraint);

   g_server->active_constraint = constraint;
   wlr_pointer_constraint_v1_send_activated(constraint);
}

void
cursor_warp_to_hint(void)
{
   struct simple_client* client = NULL;
   double sx = g_server->active_constraint->current.cursor_hint.x;
   double sy = g_server->active_constraint->current.cursor_hint.y;

   get_client_from_surface(g_server->active_constraint->surface, &client, NULL);
   if(client && g_server->active_constraint->current.cursor_hint.enabled) {
      wlr_cursor_warp(g_server->cursor, NULL, sx + client->geom.x, sy + client->geom.y);
      wlr_seat_pointer_warp(g_server->active_constraint->seat, sx, sy);
   }
}

static void
destroy_pointer_constraint_notify(struct wl_listener *listener, void *data)
{
   struct simple_pointer_constraint *pointer_constraint = wl_container_of(listener, pointer_constraint, destroy);

   if(g_server->active_constraint == pointer_constraint->constraint) {
      cursor_warp_to_hint();
      g_server->active_constraint = NULL;
   }

   wl_list_remove(&pointer_constraint->destroy.link);
   free(pointer_constraint);
}

static void 
create_pointer_constraint_notify(struct wl_listener *listener, void *data)
{
   struct simple_pointer_constraint *pointer_constraint = calloc(1, sizeof(struct simple_pointer_constraint));
   pointer_constraint->constraint = data;
   LISTEN(&pointer_constraint->constraint->events.destroy, &pointer_constraint->destroy, destroy_pointer_constraint_notify);
}

//--- Input functions ----------------------------------------------------
void 
input_focus_surface(struct wlr_surface *surface) 
{
   say(DEBUG, "focus_surface");

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
         arrange_outputs();
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
   if(!client || client->fullscreen) return;

   int new_x = g_server->cursor->x - g_server->grab_x;
   int new_y = g_server->cursor->y - g_server->grab_y;

   //Don't do anything if geometry is identical
   if(client->geom.x==new_x && client->geom.y==new_y) return;

   //client->output = get_output_at(g_server->cursor->x, g_server->cursor->y);
   //client->tag = client->output->current_tag;
   
   client->geom.x = new_x;
   client->geom.y = new_y;

   set_client_geometry(client, false);
}

static void 
process_cursor_resize(uint32_t time) 
{
   struct simple_client *client = g_server->grabbed_client;
   if(!client || client->fullscreen) return;
   
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

   // don't do anything if geometry is identical
   if (client->geom.x==new_left && client->geom.y==new_top &&
         client->geom.width==(new_right-new_left) && client->geom.height==(new_bottom-new_top)) return;

   client->geom.x = new_left;
   client->geom.y = new_top;
   client->geom.width = new_right - new_left;
   client->geom.height = new_bottom - new_top;

   if(client->type==XDG_SHELL_CLIENT)
      wlr_xdg_toplevel_set_bounds(client->xdg_surface->toplevel, client->geom.width, client->geom.height);
   
   set_client_geometry(client, true);
}

static void 
process_cursor_motion(uint32_t time, struct wlr_input_device *device, double dx, double dy,
      double dx_unaccel, double dy_unaccel) 
{
   //say(DEBUG, "process_cursor_motion");

   double sx=0, sy=0, sx_confined, sy_confined;
   struct wlr_surface *surface = NULL;
   struct simple_layer_surface *lsurface = NULL;
   struct simple_client *client = NULL, *focused_client = NULL;

   struct wlr_pointer_constraint_v1 *constraint;
   // time is 0 in internal calls meant to restore point focus
   if(time>0){
      wlr_idle_notifier_v1_notify_activity(g_server->idle_notifier, g_server->seat);

      wlr_relative_pointer_manager_v1_send_relative_motion(
            g_server->relative_pointer_manager, g_server->seat, (uint64_t)time*1000,
            dx, dy, dx_unaccel, dy_unaccel);

      wl_list_for_each(constraint, &g_server->pointer_constraints->constraints, link)
         cursor_constrain(constraint);

      if(g_server->active_constraint && g_server->cursor_mode!=CURSOR_MOVE && g_server->cursor_mode!=CURSOR_RESIZE) {
         get_client_from_surface(g_server->active_constraint->surface, &client, NULL);
         if(client && g_server->active_constraint->surface == g_server->seat->pointer_state.focused_surface) {
            sx = g_server->cursor->x - client->geom.x;
            sy = g_server->cursor->y - client->geom.y;
            if(wlr_region_confine(&g_server->active_constraint->region, sx, sy, sx+dx, sy+dy, &sx_confined, &sy_confined)) {
               dx = sx_confined - sx;
               dy = sy_confined - sy;
            }

            if(g_server->active_constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED) {
               //if(!g_server->cursor_hidden) cursor_hide();
               return;
            }
         }
      }
      wlr_cursor_move(g_server->cursor, device, dx, dy);
   }

   g_server->cur_output = get_output_at(g_server->cursor->x, g_server->cursor->y);

   if(g_server->cursor_mode == CURSOR_MOVE) {
      process_cursor_move(time);
      return;
   } else if(g_server->cursor_mode == CURSOR_RESIZE) {
      process_cursor_resize(time);
      return;
   } 

   // update drag icon's position
   wlr_scene_node_set_position(&g_server->drag_icon->node, g_server->cursor->x, g_server->cursor->y);

   int ctype_focused = get_client_from_surface(g_server->seat->keyboard_state.focused_surface, &focused_client, &lsurface);
   if(g_server->cursor_mode==CURSOR_PRESSED && !g_server->seat->drag){ 
      if(ctype_focused!=-1){
         surface = g_server->seat->pointer_state.focused_surface;
         sx = g_server->cursor->x - (ctype_focused==LAYER_SHELL_CLIENT ? lsurface->geom.x : focused_client->geom.x);
         sy = g_server->cursor->y - (ctype_focused==LAYER_SHELL_CLIENT ? lsurface->geom.y : focused_client->geom.y);
      }
   }

   // Otherwise, find the client under the pointer and send the event along
   //double sx=0, sy=0;
   //struct wlr_seat *wlr_seat = g_server->seat;
   // We actually want the top level
   int ctype = get_client_at(g_server->cursor->x, g_server->cursor->y, &client, &surface, &sx, &sy);

   if(time>0 && client && ctype!=LAYER_SHELL_CLIENT && ctype_focused!=LAYER_SHELL_CLIENT 
      && client != focused_client && g_config->focus_type>0 && !g_server->seat->drag)
      focus_client(client, g_config->focus_type==RAISE);

   if(surface) {
      wlr_seat_pointer_notify_enter(g_server->seat, surface, sx, sy);
      wlr_seat_pointer_notify_motion(g_server->seat, time, sx, sy);
   } else {
      wlr_cursor_set_xcursor(g_server->cursor, g_server->cursor_manager, "left_ptr");
      wlr_seat_pointer_notify_clear_focus(g_server->seat);
   }
} 

static void
process_cursor_button(uint32_t time, struct wlr_input_device *device, uint32_t button, enum wl_pointer_button_state state)
{
   double sx, sy;
   struct wlr_surface *surface = NULL;
   struct simple_client *client = NULL;
   int ctype = get_client_at(g_server->cursor->x, g_server->cursor->y, &client, &surface, &sx, &sy);

   struct mousemap *mousemap;
   switch (state) {
      case WL_POINTER_BUTTON_STATE_RELEASED:
         // button release
         if(!(g_server->active_constraint && g_server->active_constraint->type==WLR_POINTER_CONSTRAINT_V1_LOCKED))
            wlr_cursor_set_xcursor(g_server->cursor, g_server->cursor_manager, "left_ptr");

         if(client && g_server->grabbed_client){
            struct simple_output *test_output = get_output_at(g_server->cursor->x, g_server->cursor->y);
            if(test_output->wlr_output->enabled && test_output != client->output){
               client->output = test_output; 
               client->tag = g_server->current_tag;
            }
         }

         g_server->cursor_mode = CURSOR_NORMAL;
         g_server->grabbed_client = NULL;

         //wlr_seat_pointer_notify_button(g_server->seat, event->time_msec, event->button, event->state);
         //return;
         break;
      case WL_POINTER_BUTTON_STATE_PRESSED:
         // button press
         g_server->cursor_mode = CURSOR_PRESSED;
         struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(g_server->seat);
         uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard);
         // press on desktop
         if(!client && ctype==-1) {
            say(DEBUG, "press on desktop");
            wl_list_for_each(mousemap, &g_config->mouse_bindings, link) {
               if(modifiers ^ mousemap->mask) continue;

               if(mousemap->context==CONTEXT_ROOT && button == mousemap->button){
                  mouse_function(NULL, mousemap, 0);
                  return;
               }
            }
         } else if(ctype!=LAYER_SHELL_CLIENT) { //press on client
            focus_client(client, true);
            uint32_t resize_edges = get_resize_edges(client, g_server->cursor->x, g_server->cursor->y);
            wl_list_for_each(mousemap, &g_config->mouse_bindings, link) {
               if(modifiers ^ mousemap->mask) continue;

               if(mousemap->context==CONTEXT_CLIENT && button == mousemap->button){
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
}

//--- cursor notify functions --------------------------------------------
static void 
cursor_motion_notify(struct wl_listener *listener, void *data) 
{
   //say(DEBUG, "cursor_motion_notify");
   struct wlr_pointer_motion_event *event = data;

   //wlr_cursor_move(g_server->cursor, &event->pointer->base, event->delta_x, event->delta_y);
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

   process_cursor_button(event->time_msec, &event->pointer->base, event->button, event->state);

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

//--- Tablet notify functions --------------------------------------------
static void
tablet_transform_coords(double *x, double *y)
{
   // adjust for tablet rotation
   say(DEBUG, "rotation = %d", g_config->tablet_rotation);
   if(g_config->tablet_rotation==90){
      double tmp = *x;
      *x = (double)1.0 - *y;
      *y = tmp;
   } else if(g_config->tablet_rotation==180){
      *x = (double)1.0 - *x;
      *y = (double)1.0 - *y;
   } else if(g_config->tablet_rotation==270){
      double tmp = *x;
      *x = *y;
      *y = (double)1.0 - tmp;
   }

   //adjust for boundary limit/scaling
   double x_min = g_config->tablet_boundary_x[0];
   double x_max = g_config->tablet_boundary_x[1];
   double y_min = g_config->tablet_boundary_y[0];
   double y_max = g_config->tablet_boundary_y[1];
   say(DEBUG, "boundary_x = %d %d / boundary_y = %d %d", x_min, x_max, y_min, y_max);
   *x = (*x-x_min) / (x_max - x_min);
   *y = (*y-y_min) / (y_max - y_min);
}

static void
tablet_tool_set_cursor_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "tool_set_cursor_notify");
   struct simple_tablet_tool *tool = wl_container_of(listener, tool, set_cursor);
   struct wlr_tablet_v2_event_cursor *ev = data;

   struct wlr_seat_client *focused_client = g_server->seat->pointer_state.focused_client;
   if(ev->seat_client != focused_client) return;

   wlr_cursor_set_surface(g_server->cursor, ev->surface, ev->hotspot_x, ev->hotspot_y);
}

static void
tablet_tool_destroy_notify(struct wl_listener *listener, void *data)
{
   struct simple_tablet_tool *tool = wl_container_of(listener, tool, destroy);

   wl_list_remove(&tool->destroy.link);
   wl_list_remove(&tool->set_cursor.link);
}

static void
tablet_tool_create(struct wlr_tablet_tool *wlr_tablet_tool, struct wlr_tablet_v2_tablet *tablet_v2)
{
   say(DEBUG, "tablet_tool_create");

   struct simple_tablet_tool *tool = calloc(1, sizeof(struct simple_tablet_tool));
   tool->tool_v2 = wlr_tablet_tool_create(g_server->tablet_manager, g_server->seat, wlr_tablet_tool);
   tool->tablet_v2 = tablet_v2;
   wlr_tablet_tool->data = tool;

   say(DEBUG, " -> Tablet tool capabilities: %s %s %s %s %s %s",
         wlr_tablet_tool->tilt ? "tilt":"",
         wlr_tablet_tool->pressure ? "pressure":"",
         wlr_tablet_tool->distance ? "distance":"",
         wlr_tablet_tool->rotation ? "rotation":"",
         wlr_tablet_tool->slider ? "slider":"",
         wlr_tablet_tool->wheel ? "wheel":"");

   LISTEN(&wlr_tablet_tool->events.destroy, &tool->destroy, tablet_tool_destroy_notify);
   LISTEN(&tool->tool_v2->events.set_cursor, &tool->set_cursor, tablet_tool_set_cursor_notify);

   wl_list_insert(&g_server->tablet_tools, &tool->link);
}

static void
tablet_tool_tip_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "cursor_tool_tip_notify");
   struct wlr_tablet_tool_tip_event *ev = data;
   struct simple_tablet_tool *tool = ev->tool->data;

   wlr_idle_notifier_v1_notify_activity(g_server->idle_notifier, g_server->seat);

   double sx, sy;
   struct wlr_surface *surface = NULL;
   struct simple_client *client = NULL;
   get_client_at(g_server->cursor->x, g_server->cursor->y, &client, &surface, &sx, &sy);

   if(tool && (surface || wlr_tablet_tool_v2_has_implicit_grab(tool->tool_v2))) {
      if(ev->state==WLR_TABLET_TOOL_TIP_DOWN) {
         //wlr_seat_pointer_end_grab(g_server->seat);
         wlr_tablet_v2_tablet_tool_notify_down(tool->tool_v2);
         wlr_tablet_tool_v2_start_implicit_grab(tool->tool_v2);
      } else if(ev->state==WLR_TABLET_TOOL_TIP_UP) {
         wlr_tablet_v2_tablet_tool_notify_up(tool->tool_v2);
         if(!surface)
            wlr_tablet_v2_tablet_tool_notify_proximity_out(tool->tool_v2);
      }
   }
   //process_cursor_button(ev->time_msec, &ev->tablet->base, BTN_LEFT, 
   //      ev->state==WLR_TABLET_TOOL_TIP_UP? WL_POINTER_BUTTON_STATE_RELEASED:WL_POINTER_BUTTON_STATE_PRESSED);

   wlr_seat_pointer_notify_button(g_server->seat, ev->time_msec, BTN_LEFT, ev->state);
}

static void
tablet_tool_axis_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "cursor_tool_axis_notify");
   struct wlr_tablet_tool_axis_event *ev = data;
   struct simple_tablet_tool *tool = ev->tool->data;
   
   bool change_x = ev->updated_axes & WLR_TABLET_TOOL_AXIS_X;
   bool change_y = ev->updated_axes & WLR_TABLET_TOOL_AXIS_Y;

   //say(DEBUG, "tablet->width_mm = %f / height_mm = %f", ev->tablet->width_mm, ev->tablet->height_mm);
   say(DEBUG, "ev->x = %f / ev->y = %f", ev->x, ev->y);
   double xx = tool->x = change_x?ev->x:tool->x;
   double yy = tool->y = change_y?ev->y:tool->y;
   tablet_transform_coords(&xx, &yy);
   say(DEBUG, "xx = %f / yy = %f", xx, yy);

   wlr_cursor_warp_absolute(g_server->cursor, &ev->tablet->base, xx, yy);


   double sx, sy;
   struct wlr_surface *surface = NULL;
   struct simple_client *client = NULL;
   get_client_at(g_server->cursor->x, g_server->cursor->y, &client, &surface, &sx, &sy);

   if(surface){
      if(surface != tool->tool_v2->focused_surface && !tool->tool_v2->is_down) 
         wlr_tablet_v2_tablet_tool_notify_proximity_in(tool->tool_v2, tool->tablet_v2, surface);

      wlr_tablet_v2_tablet_tool_notify_motion(tool->tool_v2, sx, sy);

      if(ev->updated_axes & WLR_TABLET_TOOL_AXIS_PRESSURE)
         wlr_tablet_v2_tablet_tool_notify_pressure(tool->tool_v2, ev->pressure);

      if(ev->updated_axes & (WLR_TABLET_TOOL_AXIS_TILT_X | WLR_TABLET_TOOL_AXIS_TILT_Y))
         wlr_tablet_v2_tablet_tool_notify_tilt(tool->tool_v2, ev->tilt_x, ev->tilt_y);

      if(ev->updated_axes & WLR_TABLET_TOOL_AXIS_ROTATION)
         wlr_tablet_v2_tablet_tool_notify_rotation(tool->tool_v2, ev->rotation);

      //wlr_seat_pointer_notify_enter(g_server->seat, surface, sx, sy);
      //wlr_seat_pointer_notify_motion(g_server->seat, ev->time_msec, sx, sy);
   } 
   //process_cursor_motion(ev->time_msec, &ev->tablet->base, ev->dx, ev->dy, ev->dx, ev->dy);
}

static void
tablet_tool_proximity_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "cursor_tool_proximity_notify");
   struct wlr_tablet_tool_proximity_event *ev = data;
   struct wlr_tablet_tool *tool = ev->tool;

   struct simple_input *input=NULL;
   struct simple_input *temp_input;
   wl_list_for_each(temp_input, &g_server->inputs, link) {
      if(temp_input->device==&ev->tablet->base){
         input = temp_input;
         break;
      }
   }

   if(!tool->data) {
      if(!ev->tablet || !input){
         say(DEBUG, "No tablet for tablet tool");
         return;
      }
      tablet_tool_create(tool, input->tablet_v2);
   }

   struct simple_tablet_tool* simple_tool = tool->data;
   if(!simple_tool){
      say(DEBUG, "tablet tool not initialized");
      return;
   }
   
   double xx = ev->x;
   double yy = ev->y;
   tablet_transform_coords(&xx, &yy);

   if(ev->state == WLR_TABLET_TOOL_PROXIMITY_IN)
      process_cursor_motion(ev->time_msec, &ev->tablet->base, xx, yy, 0, 0);
   if(ev->state == WLR_TABLET_TOOL_PROXIMITY_OUT)
      wlr_tablet_v2_tablet_tool_notify_proximity_out(simple_tool->tool_v2);

}

static void
tablet_tool_button_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "cursor_tool_button_notify");
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
      
      //
      struct simple_input *test_input=NULL;
      wl_list_for_each(test_input, &g_server->inputs, link) {
         if(test_input==input) continue;
         if(test_input->device->type==WLR_INPUT_DEVICE_KEYBOARD) break;
      }
      if(test_input)
         wlr_seat_set_keyboard(g_server->seat, test_input->keyboard);
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
   } else if (device->type == WLR_INPUT_DEVICE_TABLET) {
      say(DEBUG, "New Input: TABLET");
      input->type = INPUT_TABLET; 
      struct wlr_tablet *tablet = wlr_tablet_from_input_device(device);
      struct wlr_tablet_v2_tablet *tablet_v2 = wlr_tablet_create(g_server->tablet_manager, g_server->seat, device);
      input->tablet = tablet;
      input->tablet_v2 = tablet_v2;
      wlr_cursor_attach_input_device(g_server->cursor, input->device);
      
      /*
      if(wlr_input_device_is_libinput(device)){
         struct libinput_device *tablet_device = wlr_libinput_get_device_handle(device);
         struct libinput_device_group *tablet_group = libinput_device_get_device_group(tablet_device);
      }*/

      wlr_cursor_map_input_to_output(g_server->cursor, device, NULL);
      wlr_cursor_map_input_to_region(g_server->cursor, device, NULL);

   } else if (device->type == WLR_INPUT_DEVICE_TABLET_PAD) {
      say(DEBUG, "New Input: TABLET_PAD");
      input->type = INPUT_TABLET_PAD;

      struct wlr_tablet_pad *pad = wlr_tablet_pad_from_input_device(device);
      input->pad = pad;

      //LISTEN(&pad->events.attach, &input->pad_

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
         case WLR_INPUT_DEVICE_TABLET:
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

   // Create a cursor - wlroots utility for tracking the cursor image shown on screen
   g_server->cursor = wlr_cursor_create();
   wlr_cursor_attach_output_layout(g_server->cursor, g_server->output_layout); 

   // create a cursor manager
   g_server->cursor_manager = wlr_xcursor_manager_create(NULL, 24);
   wlr_xcursor_manager_load(g_server->cursor_manager, 1);

   g_server->relative_pointer_manager = wlr_relative_pointer_manager_v1_create(g_server->display);

   g_server->pointer_constraints = wlr_pointer_constraints_v1_create(g_server->display);
   LISTEN(&g_server->pointer_constraints->events.new_constraint, &g_server->new_pointer_constraint, create_pointer_constraint_notify);

   g_server->cursor_mode = CURSOR_NORMAL;
   LISTEN(&g_server->cursor->events.motion, &g_server->cursor_motion, cursor_motion_notify);
   LISTEN(&g_server->cursor->events.motion_absolute, &g_server->cursor_motion_abs, cursor_motion_abs_notify);
   LISTEN(&g_server->cursor->events.button, &g_server->cursor_button, cursor_button_notify);
   LISTEN(&g_server->cursor->events.axis, &g_server->cursor_axis, cursor_axis_notify);
   LISTEN(&g_server->cursor->events.frame, &g_server->cursor_frame, cursor_frame_notify);

   //tablet events
   LISTEN(&g_server->cursor->events.tablet_tool_tip, &g_server->tablet_tool_tip, tablet_tool_tip_notify);
   LISTEN(&g_server->cursor->events.tablet_tool_proximity, &g_server->tablet_tool_proximity, tablet_tool_proximity_notify);
   LISTEN(&g_server->cursor->events.tablet_tool_axis, &g_server->tablet_tool_axis, tablet_tool_axis_notify);
   LISTEN(&g_server->cursor->events.tablet_tool_button, &g_server->tablet_tool_button, tablet_tool_button_notify);

   //input method init
   wlr_input_method_manager_v2_create(g_server->display);
   wlr_text_input_manager_v3_create(g_server->display);
}
