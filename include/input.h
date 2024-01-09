#ifndef INPUT_H
#define INPUT_H

struct simple_input {
   struct wl_list link;
   struct simple_server *server;
   struct wlr_input_device *device;
   struct wlr_keyboard *keyboard;

   enum InputType type;
   struct wl_listener kb_modifiers;
   struct wl_listener kb_key;
   struct wl_listener destroy;
};

void input_init(struct simple_server*);
void input_focus_surface(struct simple_server*, struct wlr_surface*);

#endif
