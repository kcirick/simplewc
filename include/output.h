#ifndef OUTPUT_H
#define OUTPUT_H

struct simple_output {
   struct wl_list link;
   struct wlr_output *wlr_output;

   struct wl_list layer_shells[N_LAYER_SHELL_LAYERS];

   struct wl_list ipc_outputs; // ipc addition

   struct wlr_scene_rect *fullscreen_bg;

   // tags
   int fixed_tag;
   //unsigned int current_tag;
   //unsigned int visible_tags;

   struct simple_outline *outline;

   struct wl_listener frame;
   struct wl_listener request_state;
   struct wl_listener destroy;

   struct wlr_session_lock_surface_v1 *lock_surface;
   struct wl_listener lock_surface_destroy;

   struct wlr_box full_area;
   struct wlr_box usable_area;

   bool gamma_lut_changed;
};

void new_output_notify(struct wl_listener *, void *); 
void output_layout_change_notify(struct wl_listener *, void *); 

void toggleFixedTag();

struct simple_output* get_output_at(double, double);

void arrange_outputs();

#endif
