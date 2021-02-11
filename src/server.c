#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#if XWAYLAND
#include <wlr/xwayland.h>
#endif


#include "globals.h"
#include "server.h"
#include "layer.h"
#include "xdg.h"
#include "seat.h"
#if XWAYLAND
#include "xwayland.h"
#endif

struct render_data {
   struct wlr_output *output;
   struct wlr_renderer *renderer;
   struct simple_view *view;
   struct timespec *when;
};

static void render_surface(struct wlr_surface *surface, void *data, double ox, double oy) {

   struct render_data *rdata = data;
   struct simple_view *view = rdata->view;
   struct wlr_output *output = rdata->output;

   struct wlr_texture *texture = wlr_surface_get_texture(surface);
   if (!texture) return;

   ox += view->x;
   oy += view->y;

   struct wlr_box box = {
      .x = ox * output->scale,
      .y = oy * output->scale,
      .width = surface->current.width * output->scale,
      .height = surface->current.height * output->scale,
   };

   float matrix[9];
   enum wl_output_transform transform = wlr_output_transform_invert(surface->current.transform);
   wlr_matrix_project_box(matrix, &box, transform, 0, output->transform_matrix);

   wlr_render_texture_with_matrix(rdata->renderer, texture, matrix, 1);

   wlr_surface_send_frame_done(surface, rdata->when);
}

static void render_border(struct wlr_surface *surface, void *data, double ox, double oy){ 
   struct render_data *rdata = data;
   struct simple_view *view = rdata->view;
   struct wlr_output *output = rdata->output;
   struct wlr_seat* seat = view->server->seat->seat;

   float border_colour[4];
   bool focused = false;
   focused = view->surface == seat->keyboard_state.focused_surface;
   //focused = view->surface == seat->pointer_state.focused_surface;
   focused ?  
      memcpy(border_colour, view->server->config->border_colour[FOCUSED], sizeof(border_colour)) :
      memcpy(border_colour, view->server->config->border_colour[UNFOCUSED], sizeof(border_colour));

   int scale = output->scale;
   ox += view->x;
   oy += view->y;

   double border_width = 2.;
   struct wlr_box borders;
   double width = surface->current.width;
   double height = surface->current.height;
   // top border
   borders.x = (ox - border_width) * scale;
   borders.y = (oy - border_width) * scale;
   borders.width = (width + 2*border_width) *scale;
   borders.height = border_width * scale;
   wlr_render_rect(rdata->renderer, &borders, border_colour, output->transform_matrix);
   // right border
   borders.x = (ox + width) * scale;
   borders.y = (oy - border_width) * scale;
   borders.width = border_width *scale;
   borders.height = (height + 2*border_width) * scale;
   wlr_render_rect(rdata->renderer, &borders, border_colour, output->transform_matrix);
   // bottom border
   borders.x = (ox - border_width) * scale;
   borders.y = (oy + height) * scale;
   borders.width = (width + 2*border_width) *scale;
   borders.height = border_width * scale;
   wlr_render_rect(rdata->renderer, &borders, border_colour, output->transform_matrix);
   // left border
   borders.x = (ox - border_width) * scale;
   borders.y = (oy - border_width) * scale;
   borders.width = border_width *scale;
   borders.height = (height + 2*border_width) * scale;
   wlr_render_rect(rdata->renderer, &borders, border_colour, output->transform_matrix);
}

//--- Notify functions ---------------------------------------------------
static void output_frame_notify(struct wl_listener *listener, void *data) {
   struct simple_output *output = wl_container_of(listener, output, frame);
   struct wlr_renderer *renderer = output->server->renderer;

   struct timespec now;
   clock_gettime(CLOCK_MONOTONIC, &now);

   if(!wlr_output_attach_render(output->wlr_output, NULL)) return;

   struct wlr_output *wlr_output = output->wlr_output;
   wlr_renderer_begin(renderer, wlr_output->width, wlr_output->height);
   wlr_renderer_clear(renderer, output->server->config->background_colour);

   //render_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]);
   //render_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]);

   struct simple_view *view;
   double ox=0, oy=0;
   wl_list_for_each_reverse(view, &output->server->views, link) {
      if(!view->mapped) continue;

      struct render_data rdata = {
         .output = output->wlr_output,
         .view = view,
         .renderer = renderer,
         .when = &now,
      };
      
      wlr_output_layout_output_coords(view->server->output_layout, output->wlr_output, &ox, &oy);
      render_surface(view->surface, &rdata, ox, oy);
      render_border(view->surface, &rdata, ox, oy); 
   }

   //render_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]);
   //render_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]);

   wlr_output_render_software_cursors(output->wlr_output, NULL);
   wlr_renderer_end(renderer);
   wlr_output_commit(output->wlr_output);
}

static void new_output_notify(struct wl_listener *listener, void *data) {
   struct simple_server *server = wl_container_of(listener, server, new_output);
   struct wlr_output *wlr_output = data;

   struct simple_output *output = calloc(1, sizeof(struct simple_output));
   output->wlr_output = wlr_output;
   output->server = server;
   
   wl_list_insert(&server->outputs, &output->link);

   output->frame.notify = output_frame_notify;
   wl_signal_add(&wlr_output->events.frame, &output->frame);

   wl_list_init(&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]);
   wl_list_init(&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]);
   wl_list_init(&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]);
   wl_list_init(&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]);

   // set custom resolution from config file 
   if(strcmp(server->config->output->name, wlr_output->name)==0){
      wlr_output_layout_add(server->output_layout, wlr_output, 
            server->config->output->x, server->config->output->y);
      wlr_output_set_custom_mode(wlr_output, 
            server->config->output->width, server->config->output->height,
            wlr_output->refresh);
   } else {
      struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
      wlr_output_layout_add_auto(server->output_layout, wlr_output);
      if(mode) wlr_output_set_mode(wlr_output, mode);
   }

   wlr_output_create_global(wlr_output);

   //struct wlr_output_layout_output *l_output = wlr_output_layout_get(server->output_layout, wlr_output);
   //say(INFO, "Output %s : %dx%d+%d+%d", l_output->output->name,
   //      l_output->output->width, l_output->output->height, 
   //      l_output->x, l_output->y);
}

//------------------------------------------------------------------------
void initializeOutput(struct simple_server *server) {
   
   wl_list_init(&server->outputs);   
   
   server->new_output.notify = new_output_notify;
   wl_signal_add(&server->backend->events.new_output, &server->new_output);

   server->output_layout = wlr_output_layout_create();

}

void initializeViews(struct simple_server *server) {

   wl_list_init(&server->views);

   server->xdg_shell = wlr_xdg_shell_create(server->display);
   if(!server->xdg_shell)
      say(ERROR, "unable to create XDG shell interface");

   server->new_xdg_surface.notify = new_xdg_surface_notify;
   wl_signal_add(&server->xdg_shell->events.new_surface, &server->new_xdg_surface);
#if XWAYLAND
   server->xwayland = wlr_xwayland_create(server->display, server->compositor, true);
   if(!server->xwayland)
      say(ERROR, "cannot create xwayland server");

   server->new_xwayland_surface.notify = new_xwayland_surface_notify;
   wl_signal_add(&server->xwayland->events.new_surface, &server->new_xwayland_surface);
#endif
}

static bool view_at(struct simple_view *view, double lx, double ly, struct wlr_surface **surface, double *sx, double *sy) {
   double view_sx = lx - view->x;
   double view_sy = ly - view->y;

   double _sx, _sy;
   struct wlr_surface *_surface = NULL;
#if XWAYLAND
   _surface = wlr_surface_surface_at(view->surface, view_sx, view_sy, &_sx, &_sy);
#else
   _surface = wlr_xdg_surface_surface_at(view->xdg_surface, view_sx, view_sy, &_sx, &_sy);
#endif

   if(_surface != NULL) {
      *sx = _sx;
      *sy = _sy;
      *surface = _surface;
      return true;
   }

   return false;
}

void begin_interactive(struct simple_view *view, enum cursor_mode mode, uint32_t edges) {
   struct simple_server *server = view->server;
   struct wlr_surface *focused_surface = server->seat->seat->pointer_state.focused_surface;
   // do not move/request unfocused clients
   if (view->surface != focused_surface) return;

   server->grabbed_view = view;
   server->cmode = mode;

   if(mode == CURSOR_MOVE) {
      say(INFO, "CURSOR_MOVE");
      server->grab_x = server->seat->cursor->x - view->x;
      server->grab_y = server->seat->cursor->y - view->y;
   } else {
      say(INFO, "CURSOR_RESIZE");
   }  
}

struct simple_view* desktop_view_at(struct simple_server *server, double lx, double ly, struct wlr_surface **surface, double *sx, double *sy) {
   struct simple_view *view;
   wl_list_for_each(view, &server->views, link) {
      if (view_at(view, lx, ly, surface, sx, sy))
         return view;
   }
   return NULL;
}

static void set_activated(struct wlr_surface *surface, bool activated){
   if(!surface) return;

   if(wlr_surface_is_xdg_surface(surface)) {
      struct wlr_xdg_surface *s = wlr_xdg_surface_from_wlr_surface(surface);
      wlr_xdg_toplevel_set_activated(s, activated);
#if XWAYLAND
   } else if (wlr_surface_is_xwayland_surface(surface)){
      struct wlr_xwayland_surface *s = wlr_xwayland_surface_from_wlr_surface(surface);
      wlr_xwayland_surface_activate(s, activated);
#endif
   }
}

void focus_view(struct simple_view *view) {
   if(!view) return;

   struct simple_seat *seat = view->server->seat;

   struct wlr_surface *prev_surface = seat->seat->keyboard_state.focused_surface;
   if(prev_surface==view->surface) return;

   if(prev_surface)
      set_activated(prev_surface, false);
   
   wl_list_remove(&view->link);
   wl_list_insert(&view->server->views, &view->link);

   set_activated(view->surface, true);
   seat_focus_surface(seat, view->surface);
}

//------------------------------------------------------------------------
void prepareServer(struct simple_server *server, struct simple_config *config) {
   say(INFO, "Preparing Wayland server initialization");
   
   server->display = wl_display_create();
   if(!server->display)
      say(ERROR, "Unable to create Wayland display!");

   server->backend = wlr_backend_autocreate(server->display, NULL);

   // drop permissions
   if(getuid() != geteuid() || getgid() !=getegid()){
      if(setgid(getgid()))
         say(ERROR, "unable to drop root group");
      if(setuid(getuid()))
         say(ERROR, "unable to drop root user");
   }
   if(setgid(0) != -1 || setuid(0) != -1)
      say(ERROR, "unable to drop root");

   server->config = config;

   // create renderer
   server->renderer = wlr_backend_get_renderer(server->backend);
   wlr_renderer_init_wl_display(server->renderer, server->display);
 
   // create compositor
   server->compositor = wlr_compositor_create(server->display, server->renderer);
   wlr_data_device_manager_create(server->display);

   initializeOutput(server);

   server->seat = calloc(1, sizeof(struct simple_seat));
   initializeSeat(server);

   initializeViews(server);

   initializeLayers(server);
}

void runServer(struct simple_server *server) {

   const char* socket = wl_display_add_socket_auto(server->display);
   if(!socket){
      cleanupServer(server);
      say(ERROR, "Unable to add socket to Wayland display!");
   }

   if(!wlr_backend_start(server->backend)){
      cleanupServer(server);
      say(ERROR, "Unable to start WLR backend!");
   }
   
   setenv("WAYLAND_DISPLAY", socket, true);
   
   say(INFO, "Wayland server is running on WAYLAND_DISPLAY=%s ...", socket);
   wl_display_init_shm(server->display);

#if XWAYLAND
   if(setenv("DISPLAY", server->xwayland->display_name, true) < 0)
      say(WARNING, "Unable to set DISPLAY for xwayland");
   else
      say(INFO, "XWayland is running on display %s", server->xwayland->display_name);

   wlr_xwayland_set_seat(server->xwayland, server->seat->seat);
#endif

   wl_display_run(server->display);
}

void cleanupServer(struct simple_server *server) {

#if XWAYLAND
   wlr_xwayland_destroy(server->xwayland);
#endif
   if(server->backend)
      wlr_backend_destroy(server->backend);

   wl_display_destroy_clients(server->display);
   wl_display_destroy(server->display);
   say(INFO, "Disconnected from display");

}
