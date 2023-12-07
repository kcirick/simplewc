#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>
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

#define XDG_SHELL_VERSION (3)

struct render_data {
   struct wlr_output *output;
   struct wlr_renderer *renderer;
   struct simple_view *view;
   struct timespec *when;
};

/*
static struct wl_event_source *sighup_source;
static struct wl_event_source *sigint_source;
static struct wl_event_source *sigterm_source;

static int handle_sighup(int signal, void *data){
   return 0;
}

static int handle_sigterm(int signal, void *data){
   struct wl_display *display = data;

   wl_display_terminate(display);
   return 0;
}
*/

/*
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
*/
//--- Notify functions ---------------------------------------------------
static void output_frame_notify(struct wl_listener *listener, void *data) {
   struct simple_output *output = wl_container_of(listener, output, frame);
   struct wlr_scene *scene = output->server->scene;
   struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(scene, output->wlr_output);
   
   // Render the scene if needed and commit the output 
   wlr_scene_output_commit(scene_output);
   
   struct timespec now;
   clock_gettime(CLOCK_MONOTONIC, &now);
   wlr_scene_output_send_frame_done(scene_output, &now);
}

static void output_destroy_notify(struct wl_listener *listener, void *data) {
   say(DEBUG, "output_destroy_notify");
   struct simple_output *output = wl_container_of(listener, output, destroy);

   wl_list_remove(&output->frame.link);
   wl_list_remove(&output->destroy.link);
   wl_list_remove(&output->link);
   free(output);
}

static void new_output_notify(struct wl_listener *listener, void *data) {
   say(DEBUG, "new_output_notify");
   struct simple_server *server = wl_container_of(listener, server, new_output);
   struct wlr_output *wlr_output = data;

   // Don't configure any non-desktop displays, such as VR headsets
   if(wlr_output->non_desktop) {
      say(DEBUG, "Not configuring non-desktop output");
      return;
   }

   // Configures the output created by the backend to use the allocator and renderer
   // Must be done once, before committing the output
   if(!wlr_output_init_render(wlr_output, server->allocator, server->renderer))
      say(ERROR, "unable to initialize output renderer");
   
   if(!wl_list_empty(&wlr_output->modes)){
      struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
      wlr_output_set_mode(wlr_output, mode);
      wlr_output_enable(wlr_output, true);
   }

   if (!wlr_output_test(wlr_output)) {
      say(INFO, "Preferred mode rejected, falling back to another mode");
      // TODO Add another mode
   }

   say (DEBUG, "Committing output");
   wlr_output_commit(wlr_output);

   struct simple_output *output = calloc(1, sizeof(struct simple_output));
   output->wlr_output = wlr_output;
   output->server = server;

   output->frame.notify = output_frame_notify;
   wl_signal_add(&wlr_output->events.frame, &output->frame);

   output->destroy.notify = output_destroy_notify;
   wl_signal_add(&wlr_output->events.destroy, &output->destroy);

   wl_list_insert(&server->outputs, &output->link);

   //wl_list_init(&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]);
   //wl_list_init(&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]);
   //wl_list_init(&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]);
   //wl_list_init(&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]);

   wlr_output_layout_add_auto(server->output_layout, wlr_output);

   struct wlr_output_layout_output *l_output = wlr_output_layout_get(server->output_layout, wlr_output);
   say(INFO, " -> Output %s : %dx%d+%d+%d", l_output->output->name,
         l_output->output->width, l_output->output->height, 
         l_output->x, l_output->y);
}

//------------------------------------------------------------------------
void initializeOutputLayout(struct simple_server *server) {
   server->output_layout = wlr_output_layout_create();

   wl_list_init(&server->outputs);   
   server->new_output.notify = new_output_notify;
   wl_signal_add(&server->backend->events.new_output, &server->new_output);   

   // create a scene graph
   server->scene = wlr_scene_create();
   wlr_scene_attach_output_layout(server->scene, server->output_layout);
}

void initializeXDGShell(struct simple_server *server) {

   wl_list_init(&server->views);
   server->xdg_shell = wlr_xdg_shell_create(server->display, XDG_SHELL_VERSION);
   if(!server->xdg_shell)
      say(ERROR, "unable to create XDG shell interface");

   server->xdg_new_surface.notify = xdg_new_surface_notify;
   wl_signal_add(&server->xdg_shell->events.new_surface, &server->xdg_new_surface);

   //unmanaged surfaces
 
}

void begin_interactive(struct simple_view *view, enum cursor_mode mode, uint32_t edges) {
   // this function sets up an interactive move or resize operation
   struct simple_server *server = view->server;
   struct wlr_surface *focused_surface = server->seat->seat->pointer_state.focused_surface;
   // do not move/request unfocused clients
   if(view->type==XDG_SHELL_VIEW) {
      if(view->xdg_toplevel->base->surface != wlr_surface_get_root_surface(focused_surface)) 
         return;
   } else if(view->type==XWAYLAND_VIEW){
      if(view->xwayland_surface->surface != wlr_surface_get_root_surface(focused_surface))
         return;
   }

   server->grabbed_view = view;
   server->cmode = mode;

   if(mode == CURSOR_MOVE) {
      say(INFO, "CURSOR_MOVE");
      server->grab_x = server->seat->cursor->x - view->current.x;
      server->grab_y = server->seat->cursor->y - view->current.y;
   } else {
      say(INFO, "CURSOR_RESIZE");
      struct wlr_box geo_box;
      if(view->type==XDG_SHELL_VIEW) {
         wlr_xdg_surface_get_geometry(view->xdg_toplevel->base, &geo_box);
         geo_box.x = view->current.x;
         geo_box.y = view->current.y;
#if XWAYLAND
      } else {
         geo_box = view->current;
#endif
      }

      server->grab_x = server->seat->cursor->x;
      server->grab_y = server->seat->cursor->y;
      server->grab_box = geo_box;

      server->resize_edges = edges;
   }  
}

struct simple_view* desktop_view_at(struct simple_server *server, double lx, double ly, struct wlr_surface **surface, double *sx, double *sy) {
   //say(DEBUG, "desktop_view_at()");
   struct wlr_scene_node *node = wlr_scene_node_at(&server->scene->tree.node, lx, ly, sx, sy);
   if(node == NULL || node->type != WLR_SCENE_NODE_BUFFER) return NULL;

   struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
   struct wlr_scene_surface *scene_surface = wlr_scene_surface_from_buffer(scene_buffer);
   if(!scene_surface) return NULL;

   *surface = scene_surface->surface;

   struct wlr_scene_tree *tree = node->parent;
   while(tree != NULL && tree->node.data==NULL)
      tree = tree->node.parent;

   //say(DEBUG, "desktop_view_at(): found node parent");
   return tree->node.data;
}

static void set_activated(struct wlr_surface *surface, bool activated){
   if(!surface) return;

   if(wlr_surface_is_xdg_surface(surface)) {
      struct wlr_xdg_surface *s = wlr_xdg_surface_from_wlr_surface(surface);
      wlr_xdg_toplevel_set_activated(s->toplevel, activated);
#if XWAYLAND
      
   } else if (wlr_surface_is_xwayland_surface(surface)){
      struct wlr_xwayland_surface *s = wlr_xwayland_surface_from_wlr_surface(surface);
      wlr_xwayland_surface_activate(s, activated);
      
#endif
   }
}

void focus_view(struct simple_view *view, struct wlr_surface *surface) {
   say(DEBUG, "focus_view()");
   if(!view) return;

   struct simple_server *server = view->server;
   struct simple_seat *seat = server->seat;

   struct wlr_surface *prev_surface = seat->seat->keyboard_state.focused_surface;
   if(prev_surface==surface) return;

   say(DEBUG, "HERE 12");
   if(prev_surface){
      //deactivate the previously focused surface.
      //struct wlr_xdg_surface *previous = wlr_xdg_surface_from_wlr_surface(seat->seat->keyboard_state.focused_surface);
      //assert(previous->role==WLR_XDG_SURFACE_ROLE_TOPLEVEL);
      //wlr_xdg_toplevel_set_activated(previous->toplevel, false);
      set_activated(prev_surface, false);
   }
   
   say(DEBUG, "HERE 20");
   wlr_scene_node_raise_to_top(&view->scene_tree->node);
   wl_list_remove(&view->link);
   wl_list_insert(&view->server->views, &view->link);

   say(DEBUG, "HERE 24");
   set_activated(surface, true);
   //wlr_xdg_toplevel_set_activated(view->xdg_toplevel, true);
   //seat_focus_surface(seat, view->xdg_toplevel->base->surface);
   seat_focus_surface(seat, surface);
   say(DEBUG, "End focus_view()");
}

//------------------------------------------------------------------------
void prepareServer(struct simple_server *server, struct simple_config *config) {
   say(INFO, "Preparing Wayland server initialization");
   
   server->display = wl_display_create();
   if(!server->display)
      say(ERROR, "Unable to create Wayland display!");

   /*
   // Catch SIGHUP
   struct wl_event_loop *event_loop = NULL;
   event_loop = wl_display_get_event_loop(server->display);
   sighup_source  = wl_event_loop_add_signal(event_loop, SIGHUP, handle_sighup, NULL);
   sigint_source  = wl_event_loop_add_signal(event_loop, SIGINT, handle_sigterm, server->display);
   sigterm_source = wl_event_loop_add_signal(event_loop, SIGTERM, handle_sigterm, server->display);
   //server->wl_event_loop = event_loop;
   */

   server->backend = wlr_backend_autocreate(server->display);
   if(!server->backend)
      say(ERROR, "Unable to create wlr_backend!");

   /*
   // drop permissions
   if(getuid() != geteuid() || getgid() !=getegid()){
      if(setgid(getgid()))
         say(ERROR, "unable to drop root group");
      if(setuid(getuid()))
         say(ERROR, "unable to drop root user");
   }
   if(setgid(0) != -1 || setuid(0) != -1)
      say(ERROR, "unable to drop root");
   */

   server->config = config;

   // create renderer
   server->renderer = wlr_renderer_autocreate(server->backend);
   if(!server->renderer)
      say(ERROR, "Unable to create wlr_renderer");

   wlr_renderer_init_wl_display(server->renderer, server->display);
 
   // create an allocator
   server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);
   if(!server->allocator)
      say(ERROR, "Unable to create wlr_allocator");

   // create compositor
   server->compositor = wlr_compositor_create(server->display, server->renderer);
   wlr_subcompositor_create(server->display);
   wlr_data_device_manager_create(server->display);

   // create an output layout, i.e. wlroots utility for working with an arrangement of 
   // screens in a physical layout
   initializeOutputLayout(server);
   
   // set up xdg-shell version 3.
   initializeXDGShell(server);

   // set up seat and inputs
   initializeSeat(server);

   //initializeLayers(server);

#if XWAYLAND
   initializeXWayland(server);
#endif

}

void startServer(struct simple_server *server) {

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
   //wl_display_init_shm(server->display);

#if XWAYLAND
   startXWayland(server);
#endif
}

void cleanupServer(struct simple_server *server) {
   
   say(DEBUG, "cleanupServer");
#if XWAYLAND
   server->xwayland = NULL;
   wlr_xwayland_destroy(server->xwayland);
#endif
   if(server->backend)
      wlr_backend_destroy(server->backend);

   wl_display_destroy_clients(server->display);
   wl_display_destroy(server->display);
   say(INFO, "Disconnected from display");
}
