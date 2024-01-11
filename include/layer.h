#ifndef LAYER_H
#define LAYER_H

struct simple_layer_surface {
   struct wl_list link;
   struct simple_output *output;
   enum ClientType type;

   //struct wlr_layer_surface_v1 *layer_surface;
   struct wlr_scene_layer_surface_v1 *scene_layer_surface;
   struct wlr_scene_tree *scene_tree;
   struct wlr_scene_tree *popups;

   struct wl_listener map;
   struct wl_listener unmap;
   struct wl_listener destroy;
   struct wl_listener surface_commit;
   struct wl_listener new_popup;

   bool mapped;

   // geometry of the wlr_surface within the view as currently displayed
   struct wlr_box geom;
};

void arrange_layers(struct simple_output*);
void layer_new_surface_notify(struct wl_listener*, void*);

#endif
