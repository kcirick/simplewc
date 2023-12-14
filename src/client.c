//#include <unistd.h>
#include <assert.h>
#include <wayland-server-core.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <xkbcommon/xkbcommon.h>
#if XWAYLAND
#include <wlr/xwayland.h>
#endif

#include "globals.h"
#include "client.h"
#include "server.h"
#include "layer.h"


void 
begin_interactive(struct simple_client *client, enum cursor_mode mode, uint32_t edges)
{
   // this function sets up an interactive move or resize operation
   struct simple_server *server = client->server;
   struct wlr_surface *focused_surface = server->seat->pointer_state.focused_surface;
   
   // do not move/request unfocused clients
   if(client->type==XDG_SHELL_CLIENT) {
      if(client->xdg_surface->surface != wlr_surface_get_root_surface(focused_surface)) 
         return;
   } else if(client->type==XWL_MANAGED_CLIENT){
      if(client->xwayland_surface->surface != wlr_surface_get_root_surface(focused_surface))
         return;
   }

   server->grabbed_client = client;
   server->cursor_mode = mode;

   if(mode == CURSOR_MOVE) {
      say(INFO, "CURSOR_MOVE");
      server->grab_x = server->cursor->x - client->geom.x;
      server->grab_y = server->cursor->y - client->geom.y;
   } else {
      say(INFO, "CURSOR_RESIZE");
      struct wlr_box geo_box;
      if(client->type==XDG_SHELL_CLIENT) {
         wlr_xdg_surface_get_geometry(client->xdg_surface, &geo_box);
         geo_box.x = client->geom.x;
         geo_box.y = client->geom.y;
#if XWAYLAND
      } else {
         geo_box = client->geom;
#endif
      }

      server->grab_x = server->cursor->x;
      server->grab_y = server->cursor->y;
      server->grab_box = geo_box;

      server->resize_edges = edges;
   }  
}

struct simple_client* 
get_client_at(struct simple_server *server, double lx, double ly, struct wlr_surface **surface, double *sx, double *sy) 
{
   //say(DEBUG, "client_at()");
   struct wlr_scene_node *node = wlr_scene_node_at(&server->scene->tree.node, lx, ly, sx, sy);
   if(node == NULL || node->type != WLR_SCENE_NODE_BUFFER) return NULL;

   struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
   struct wlr_scene_surface *scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);
   if(!scene_surface) return NULL;

   *surface = scene_surface->surface;

   struct wlr_scene_tree *tree = node->parent;
   while(tree != NULL && tree->node.data==NULL)
      tree = tree->node.parent;

   //say(DEBUG, "client_at(): found node parent");
   if(((struct simple_client*)tree->node.data)->type == LAYER_SHELL_CLIENT){
      return NULL;
   } else
      return tree->node.data;
}

void 
set_client_activated(struct simple_client *client, bool activated)
{
   if(!client) return;

   if(client->type==XDG_SHELL_CLIENT) {
      struct wlr_xdg_toplevel *tl = client->xdg_surface->toplevel;
      wlr_xdg_toplevel_set_activated(tl, activated);
#if XWAYLAND
   } else if(client->type==XWL_MANAGED_CLIENT) {
      struct wlr_xwayland_surface *xwl_s = client->xwayland_surface;
      wlr_xwayland_surface_activate(xwl_s, activated);
#endif
   }
}

int 
get_client_from_surface(struct wlr_surface *surface, struct simple_client **client, struct simple_layer_surface **lsurface)
{
   if(!surface) return -1;
   
   struct wlr_surface *root_surface = wlr_surface_get_root_surface(surface);
   int type = -1;

#ifdef XWAYLAND
   struct wlr_xwayland_surface *xs = wlr_xwayland_surface_try_from_wlr_surface(root_surface);
   if (xs){
      say(DEBUG, "XS");
      *client = xs->data;
      type = (*client)->type;
      return type;
   }
#endif

   struct wlr_layer_surface_v1 *ls = wlr_layer_surface_v1_try_from_wlr_surface(root_surface);
   if(ls) {
      say(DEBUG, "XS");
      *lsurface = ls->data; 
      type = (*lsurface)->type;
      return type;
   }

   struct wlr_xdg_surface *tmp_s;
   struct wlr_xdg_surface *s = wlr_xdg_surface_try_from_wlr_surface(root_surface);
   while(s) {
      tmp_s = NULL;
      switch(s->role){
         case WLR_XDG_SURFACE_ROLE_POPUP:
            say(DEBUG, "POPUP");
            if(!s->popup || !s->popup->parent)
               return -1;
            tmp_s = wlr_xdg_surface_try_from_wlr_surface(s->popup->parent);

            if(!tmp_s) {
               return get_client_from_surface(s->popup->parent, client, lsurface);
            }
            s = tmp_s;
            break;
         case WLR_XDG_SURFACE_ROLE_TOPLEVEL:
            say(DEBUG, "TOPLEVEL");
            *client = s->data;
            type = (*client)->type;
            return type;
         case WLR_XDG_SURFACE_ROLE_NONE:
            return -1;
      }
   }
   return -1;
}

void
get_client_size(struct simple_client *client, struct wlr_box *geom)
{
   if(client->type==XDG_SHELL_CLIENT){
      wlr_xdg_surface_get_geometry(client->xdg_surface, geom);
#if XWAYLAND
   } else {
      geom->x = client->xwayland_surface->x;
      geom->y = client->xwayland_surface->y;
      geom->width = client->xwayland_surface->width;
      geom->height = client->xwayland_surface->height;
#endif
   }
}

void 
set_client_size(struct simple_client *client, struct wlr_box geom) 
{
   struct simple_config *config = client->server->config;
   if(client->type==XDG_SHELL_CLIENT){
      wlr_xdg_toplevel_set_size(client->xdg_surface->toplevel, geom.width, geom.height);
#if XWAYLAND
   } else {
      wlr_xwayland_surface_configure(client->xwayland_surface, 
            geom.x + config->border_width, 
            geom.y + config->border_width, 
            geom.width, 
            geom.height);
#endif
   }
   //borders
   wlr_scene_rect_set_size(client->border[0], client->geom.width, config->border_width);
   wlr_scene_rect_set_size(client->border[1], client->geom.width, config->border_width);
   wlr_scene_node_set_position(&client->border[1]->node, 0, client->geom.height - config->border_width);
   wlr_scene_rect_set_size(client->border[2], config->border_width, client->geom.height - 2 * config->border_width);
   wlr_scene_node_set_position(&client->border[2]->node, 0, config->border_width);
   wlr_scene_rect_set_size(client->border[3], config->border_width, client->geom.height - 2 * config->border_width);
   wlr_scene_node_set_position(&client->border[3]->node, client->geom.width - config->border_width, config->border_width);
}

void 
set_client_border_colour(struct simple_client *client, int colour) 
{
   struct simple_config *config = client->server->config;
   for (int i=0; i<4; i++)
      wlr_scene_rect_set_color(client->border[i], config->border_colour[colour]);
}

void 
focus_client(struct simple_client *client, struct wlr_surface *surface) 
{
   say(DEBUG, "focus_client()");
   if(!client) return;

   say(DEBUG, "HERE 10");
   struct simple_server *server = client->server;
   int old_client_type;
   struct simple_client* old_client=NULL;
   struct simple_layer_surface* old_lsurface = NULL;

   say(DEBUG, "HERE 20");
   struct wlr_surface *prev_surface = server->seat->keyboard_state.focused_surface;
   if(prev_surface==surface) return;

   say(DEBUG, "HERE 30");
   if(prev_surface){
      old_client_type = get_client_from_surface(prev_surface, &old_client, &old_lsurface);
      if(old_client_type == XDG_SHELL_CLIENT || old_client_type == XWL_MANAGED_CLIENT){
         //deactivate the previously focused surface.
         set_client_activated(old_client, false);
         set_client_border_colour(old_client, UNFOCUSED);
      }
   }
   say(DEBUG, "HERE 40");
   
   wlr_scene_node_raise_to_top(&client->scene_tree->node);
   wl_list_remove(&client->link);
   wl_list_insert(&client->server->clients, &client->link);

   set_client_activated(client, true);
   set_client_border_colour(client, FOCUSED);

   input_focus_surface(server, surface);
}

void 
set_initial_geometry(struct simple_client* client) 
{
   if(wlr_box_empty(&client->geom))
      get_client_size(client, &client->geom);

   say(DEBUG, " -> Initial geometry : %d %d %d %d", client->geom.x, client->geom.y, client->geom.width, client->geom.height);
   // borders
   for(int i=0; i<4; i++){
      client->border[i] = wlr_scene_rect_create(client->scene_tree, 0, 0, client->server->config->border_colour[FOCUSED]);
      client->border[i]->node.data = client;
   }

   set_client_size(client, client->geom);
}

// --- Common notify functions -------------------------------------------
static void 
map_notify(struct wl_listener *listener, void *data) 
{
   say(INFO, "map_notify");
   struct simple_client *client = wl_container_of(listener, client, map);
   if(client->mapped) return;

   wl_list_insert(&client->server->clients, &client->link);
   client->mapped = true;

   struct wlr_scene_tree *tree = client->type==XDG_SHELL_CLIENT ?
      wlr_scene_xdg_surface_create(client->scene_tree, client->xdg_surface) :
      wlr_scene_subsurface_tree_create(client->scene_tree, client->xwayland_surface->surface);
   if(!tree) return;
   client->scene_node = &tree->node;

   set_initial_geometry(client);
   struct wlr_output *output = wlr_output_layout_output_at(client->server->output_layout, client->geom.x, client->geom.y);
   if(output)
      client->output = output->data;

   focus_client(client, client->type==XDG_SHELL_CLIENT ? client->xdg_surface->surface : client->xwayland_surface->surface);
}

static void 
unmap_notify(struct wl_listener *listener, void *data) 
{
   say(INFO, "unmap_notify");
   struct simple_client *client = wl_container_of(listener, client, unmap);

   // reset the cursor mode if the grabbed client was unmapped
   if(client == client->server->grabbed_client) {
      client->server->cursor_mode = CURSOR_PASSTHROUGH;
      client->server->grabbed_client = NULL;
   }
   
   wl_list_remove(&client->link);
   client->mapped = false;

   wlr_scene_node_destroy(&client->scene_tree->node);
}

static void 
destroy_notify(struct wl_listener *listener, void *data) 
{
   say(INFO, "destroy_notify");
   struct simple_client *client = wl_container_of(listener, client, destroy);

   wl_list_remove(&client->destroy.link);
   if(client->type==XDG_SHELL_CLIENT){
      wl_list_remove(&client->map.link);
      wl_list_remove(&client->unmap.link);
      wl_list_remove(&client->request_move.link);
      wl_list_remove(&client->request_resize.link);
   } else {
      wl_list_remove(&client->associate.link);
      wl_list_remove(&client->dissociate.link);
      wl_list_remove(&client->request_activate.link);
      wl_list_remove(&client->request_configure.link);
   }

   free(client);
}

// --- XDG Shell ---------------------------------------------------------
static void 
xdg_tl_request_move_notify(struct wl_listener *listener, void *data) 
{
   struct simple_client *client = wl_container_of(listener, client, request_move);
   begin_interactive(client, CURSOR_MOVE, 0);
}

static void 
xdg_tl_request_resize_notify(struct wl_listener *listener, void *data)
{
   struct simple_client *client = wl_container_of(listener, client, request_resize);
   struct wlr_xdg_toplevel_resize_event *event = data;
   begin_interactive(client, CURSOR_RESIZE, event->edges);
}

void 
xdg_new_surface_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "new_xdg_surface_notify");
   struct simple_server *server = wl_container_of(listener, server, xdg_new_surface);
   struct wlr_xdg_surface *xdg_surface = data;
   struct simple_client* pclient;
   struct simple_layer_surface* lsurface;

   // add xdg popups to the scene graph
   if(xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
      struct wlr_box box;
      int type = get_client_from_surface(xdg_surface->surface, &pclient, &lsurface);
      if(!xdg_surface->popup->parent || type<0)
         return;
      xdg_surface->surface->data = wlr_scene_xdg_surface_create(
            type == LAYER_SHELL_CLIENT ? lsurface->scene_tree : pclient->scene_tree, xdg_surface);
      box = type == LAYER_SHELL_CLIENT ? lsurface->output->usable_area : pclient->output->usable_area;
      //box.x -= ...
      //box.y -= ...
      wlr_xdg_popup_unconstrain_from_box(xdg_surface->popup, &box);
      /*
      struct wlr_xdg_surface *parent = wlr_xdg_surface_try_from_wlr_surface(xdg_surface->popup->parent);
      assert(parent != NULL);
      struct simple_client *pclient = parent->data;
      struct wlr_scene_tree *parent_tree = pclient->scene_tree;
      wlr_scene_xdg_surface_create(parent_tree, xdg_surface);
      xdg_surface->data = pclient; 
      */
      return;
   }
   assert(xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);

   // allocate a simple_client for this surface
   struct simple_client *client = calloc(1, sizeof(struct simple_client));
   client->server = server;
   client->type = XDG_SHELL_CLIENT;
   client->xdg_surface = xdg_surface;
   client->scene_tree = wlr_scene_tree_create(&client->server->scene->tree);
   client->scene_tree->node.data = client;
   //xdg_surface->data = client->scene_tree;
   xdg_surface->data = client;

   LISTEN(&xdg_surface->surface->events.map, &client->map, map_notify);
   LISTEN(&xdg_surface->surface->events.unmap, &client->unmap, unmap_notify);
   LISTEN(&xdg_surface->surface->events.destroy, &client->destroy, destroy_notify);

   struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;
   LISTEN(&toplevel->events.request_move, &client->request_move, xdg_tl_request_move_notify);
   LISTEN(&toplevel->events.request_resize, &client->request_resize, xdg_tl_request_resize_notify);
   // more...
}

//---- XWayland Shell ----------------------------------------------------
static void 
xwl_associate_notify(struct wl_listener *listener, void *data) 
{
   say(DEBUG, "xwl_associate_notify");
   struct simple_client *xwl_client = wl_container_of(listener, xwl_client, associate);

   LISTEN(&xwl_client->xwayland_surface->surface->events.map, &xwl_client->map, map_notify);
   LISTEN(&xwl_client->xwayland_surface->surface->events.unmap, &xwl_client->unmap, unmap_notify);
}

static void 
xwl_dissociate_notify(struct wl_listener *listener, void *data) 
{
   say(DEBUG, "xwl_dissociate_notify");
   struct simple_client *xwl_client = wl_container_of(listener, xwl_client, dissociate);

   wl_list_remove(&xwl_client->map.link);
   wl_list_remove(&xwl_client->unmap.link);
}

static void 
xwl_request_activate_notify(struct wl_listener *listener, void *data) 
{
   say(INFO, "xwl_request_activate_notify");
}

static void 
xwl_request_configure_notify(struct wl_listener *listener, void *data) 
{
   say(INFO, "xwl_request_configure_notify");
   struct simple_client *client = wl_container_of(listener, client, request_configure);
   struct wlr_xwayland_surface_configure_event *event = data;
   wlr_xwayland_surface_configure(client->xwayland_surface, event->x, event->y, event->width, event->height);
}

//------------------------------------------------------------------------
void 
xwl_unmanaged_create(struct simple_server *server, struct wlr_xwayland_surface *xsurface) 
{
   //
   say(DEBUG, "xwayland_unmanaged_create");
}

void 
xwl_ready_notify(struct wl_listener *listener, void *data) 
{
   say(DEBUG, "xwl_ready_notify");
   struct simple_server *server = wl_container_of(listener, server, xwl_ready);

   wlr_xwayland_set_seat(server->xwayland, server->seat);
   
   struct wlr_xcursor *xcursor;
   xcursor = wlr_xcursor_manager_get_xcursor(server->cursor_manager, "left_ptr", 1);
   if(xcursor){
      struct wlr_xcursor_image *image = xcursor->images[0];
      wlr_xwayland_set_cursor(server->xwayland, image->buffer, 
            image->width*4, image->width, image->height, image->hotspot_x, image->hotspot_y);
   }
}

void 
xwl_new_surface_notify(struct wl_listener *listener, void *data) 
{
   say(INFO, "xwayland_new_surface_notify");
   struct simple_server *server = wl_container_of(listener, server, xwl_new_surface);
   struct wlr_xwayland_surface *xsurface = data;
   wlr_xwayland_surface_ping(xsurface);

   if(xsurface->override_redirect){
      xwl_unmanaged_create(server, xsurface);
      return;
   }

   // Create simple_client for this surface 
   struct simple_client *xwl_client = calloc(1, sizeof(struct simple_client));

   xwl_client->server = server;
   xwl_client->type = XWL_MANAGED_CLIENT;
   xwl_client->xwayland_surface = xsurface;
   xwl_client->scene_tree = wlr_scene_tree_create(&xwl_client->server->scene->tree);
   xwl_client->scene_tree->node.data = xwl_client;
   //xsurface->data = xwl_client->scene_tree;
   xsurface->data = xwl_client;

   LISTEN(&xsurface->events.associate, &xwl_client->associate, xwl_associate_notify);
   LISTEN(&xsurface->events.dissociate, &xwl_client->dissociate, xwl_dissociate_notify);
   LISTEN(&xsurface->events.destroy, &xwl_client->destroy, destroy_notify);

   LISTEN(&xsurface->events.request_activate, &xwl_client->request_activate, xwl_request_activate_notify);
   LISTEN(&xsurface->events.request_configure, &xwl_client->request_configure, xwl_request_configure_notify);

   // More mappings
}

