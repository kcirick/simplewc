#ifndef LAYER_H
#define LAYER_H

struct simple_layer_surface {
   struct wl_list link;
   //struct wlr_layer_surface_v1 *layer_surface;
   struct wlr_scene_layer_surface_v1 *scene_layer_surface;
   struct simple_server *server;

   struct wl_listener map;
   struct wl_listener unmap;
   struct wl_listener destroy;
   struct wl_listener surface_commit;

   bool mapped;

   // geometry of the wlr_surface within the view as currently displayed
   struct wlr_box geom;
};

void layer_new_surface_notify(struct wl_listener*, void*);

//void initializeLayers(struct simple_server*);
//void render_layer(struct simple_output *, struct wl_list *);

#endif
