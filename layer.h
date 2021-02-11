#ifndef LAYER_H
#define LAYER_H

struct simple_layer_surface {
   struct wl_list link;
   struct wlr_layer_surface_v1 *layer_surface;
   struct simple_server *server;

   struct wl_listener destroy;
   struct wl_listener map;
   struct wl_listener unmap;
   struct wl_listener surface_commit;
   struct wl_listener output_destroy;

   struct wlr_box geom;
};

void initializeLayers(struct simple_server*);
void render_layer(struct simple_output *, struct wl_list *);

#endif
