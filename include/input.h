#ifndef INPUT_H
#define INPUT_H

struct simple_input {
   struct wl_list link;
   struct wlr_input_device *device;

   enum InputType type;

   // common handlers
   struct wl_listener destroy;

   //keyboard handlers
   struct wlr_keyboard *keyboard;

   struct wl_listener kb_modifiers;
   struct wl_listener kb_key;

   //tablet handlers
   struct wlr_tablet *tablet;
   struct wlr_tablet_v2_tablet *tablet_v2;

   //tablet pad handlers
   struct wlr_tablet_pad *pad;
};

struct simple_pointer_constraint {
   struct wlr_pointer_constraint_v1 *constraint;
   struct wl_listener destroy;
};

struct simple_tablet_tool {
   struct wl_list link;
   struct wlr_tablet_v2_tablet_tool *tool_v2;
   struct wlr_tablet_v2_tablet *tablet_v2;

   double x, y;

   struct wl_listener set_cursor;
   struct wl_listener destroy;
};

void input_focus_surface(struct wlr_surface*);

void input_init();

#endif
