#ifndef INPUT_H
#define INPUT_H

struct simple_input {
   struct wl_list link;
   struct wlr_input_device *device;
   struct wlr_keyboard *keyboard;

   enum InputType type;
   struct wl_listener kb_modifiers;
   struct wl_listener kb_key;
   struct wl_listener destroy;
};

struct simple_pointer_constraint {
   struct wlr_pointer_constraint_v1 *constraint;
   struct wl_listener destroy;
};

void input_focus_surface(struct wlr_surface*);

void input_init();

#endif
