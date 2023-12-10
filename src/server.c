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
//#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/edges.h>
#if XWAYLAND
#include <wlr/xwayland.h>
#endif

#include "globals.h"
#include "client.h"
#include "server.h"
#include "layer.h"
#include "seat.h"

#define XDG_SHELL_VERSION (3)

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

//--- Notify functions ---------------------------------------------------
static void output_frame_notify(struct wl_listener *listener, void *data) {
   say(DEBUG, "output_frame_notify");
   struct simple_output *output = wl_container_of(listener, output, frame);
   struct wlr_scene *scene = output->server->scene;
   struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(scene, output->wlr_output);
   
   // Render the scene if needed and commit the output 
   wlr_scene_output_commit(scene_output, NULL);
   
   struct timespec now;
   clock_gettime(CLOCK_MONOTONIC, &now);
   wlr_scene_output_send_frame_done(scene_output, &now);
}

static void output_request_state_notify(struct wl_listener *listener, void *data) {
   say(DEBUG, "output_request_state_notify");
   // called when the backend requests a new state for the output
   struct simple_output *output = wl_container_of(listener, output, request_state);
   const struct wlr_output_event_request_state *event = data;
   wlr_output_commit_state(output->wlr_output, event->state);
}

static void output_destroy_notify(struct wl_listener *listener, void *data) {
   say(DEBUG, "output_destroy_notify");
   struct simple_output *output = wl_container_of(listener, output, destroy);

   wl_list_remove(&output->frame.link);
   wl_list_remove(&output->request_state.link);
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
   
   // The output may be disabled. Switch it on
   struct wlr_output_state state;
   wlr_output_state_init(&state);
   wlr_output_state_set_enabled(&state, true);

   struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
   if (mode)
      wlr_output_state_set_mode(&state, mode);

   wlr_output_commit_state(wlr_output, &state);
   wlr_output_state_finish(&state);

   struct simple_output *output = calloc(1, sizeof(struct simple_output));
   output->wlr_output = wlr_output;
   output->server = server;

   output->frame.notify = output_frame_notify;
   wl_signal_add(&wlr_output->events.frame, &output->frame);
   output->request_state.notify = output_request_state_notify;
   wl_signal_add(&wlr_output->events.request_state, &output->request_state);
   output->destroy.notify = output_destroy_notify;
   wl_signal_add(&wlr_output->events.destroy, &output->destroy);

   wl_list_insert(&server->outputs, &output->link);

   //wl_list_init(&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]);
   //wl_list_init(&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]);
   //wl_list_init(&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]);
   //wl_list_init(&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]);

   struct wlr_output_layout_output *l_output =
      wlr_output_layout_add_auto(server->output_layout, wlr_output);
   struct wlr_scene_output *scene_output =
      wlr_scene_output_create(server->scene, wlr_output);
   wlr_scene_output_layout_add_output(server->scene_layout, l_output, scene_output);

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
   server->scene_layout = wlr_scene_attach_output_layout(server->scene, server->output_layout);
}

void initializeXDGShell(struct simple_server *server) {

   wl_list_init(&server->clients);
   server->xdg_shell = wlr_xdg_shell_create(server->display, XDG_SHELL_VERSION);
   if(!server->xdg_shell)
      say(ERROR, "unable to create XDG shell interface");

   server->xdg_new_surface.notify = xdg_new_surface_notify;
   wl_signal_add(&server->xdg_shell->events.new_surface, &server->xdg_new_surface);

   //unmanaged surfaces
 
}

#if XWAYLAND
void initializeXWayland(struct simple_server *server) {

   server->xwayland = wlr_xwayland_create(server->display, server->compositor, true);
   if(!server->xwayland)
      say(ERROR, "unable to create xwayland server");

   server->xwl_new_surface.notify = xwl_new_surface_notify;
   wl_signal_add(&server->xwayland->events.new_surface, &server->xwl_new_surface);

   server->xwl_ready.notify = xwl_ready_notify;
   wl_signal_add(&server->xwayland->events.ready, &server->xwl_ready);

}
#endif

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

   server->backend = wlr_backend_autocreate(server->display, NULL);
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
   server->compositor = wlr_compositor_create(server->display, 5, server->renderer);
   wlr_subcompositor_create(server->display);
   wlr_data_device_manager_create(server->display);

   // create an output layout, i.e. wlroots utility for working with an arrangement of 
   // screens in a physical layout
   initializeOutputLayout(server);
   
   // set up xdg-shell 
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

#if XWAYLAND
   if(setenv("DISPLAY", server->xwayland->display_name, true) < 0)
      say(WARNING, " -> Unable to set DISPLAY for xwayland");
   else 
      say(INFO, " -> XWayland is running on display %s", server->xwayland->display_name);
   
   struct wlr_xcursor *xcursor;
   xcursor = wlr_xcursor_manager_get_xcursor(server->seat->cursor_manager, "left_ptr", 1);
   if(xcursor){
      struct wlr_xcursor_image *image = xcursor->images[0];
      wlr_xwayland_set_cursor(server->xwayland, image->buffer, 
            image->width*4, image->width, image->height, image->hotspot_x, image->hotspot_y);
   }
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
   wlr_scene_node_destroy(&server->scene->tree.node);
   wlr_xcursor_manager_destroy(server->seat->cursor_manager);
   wlr_output_layout_destroy(server->output_layout);
   wl_display_destroy(server->display);
   say(INFO, "Disconnected from display");
}
