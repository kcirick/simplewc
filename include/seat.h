#ifndef SEAT_H
#define SEAT_H

struct simple_seat {
   struct wlr_seat *seat;
   struct simple_server *server;

   struct wl_list inputs;   
   struct wl_listener new_input;

   struct wlr_cursor *cursor;
   struct wlr_xcursor_manager *cursor_manager;
   struct wl_listener cursor_motion;
   struct wl_listener cursor_motion_abs;
   struct wl_listener cursor_button;
   struct wl_listener cursor_axis;
   struct wl_listener cursor_frame;

   struct wl_listener request_cursor;
   struct wl_listener request_set_selection;
};

struct simple_input {
   struct wl_list link;
   struct simple_seat *seat;
   struct wlr_input_device *device;
   struct wl_listener destroy;

   struct wl_listener kb_modifiers;
   struct wl_listener kb_key;
};

void initializeSeat(struct simple_server*); 
void seat_focus_surface(struct simple_seat*, struct wlr_surface*);

#endif
