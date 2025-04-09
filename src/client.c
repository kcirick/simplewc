#include <assert.h>
#include <string.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>

#include "globals.h"
#include "layer.h"
#include "input.h"
#include "client.h"
#include "server.h"
#include "output.h"

static inline struct wlr_surface*
get_client_surface(struct simple_client *client)
{
#if XWAYLAND
   if(client->type==XWL_MANAGED_CLIENT || client->type==XWL_UNMANAGED_CLIENT)
      return client->xwl_surface->surface;
#endif
   return client->xdg_surface->surface;
}

//--- Action calls -------------------------------------------------------
void
sendClientToTag(struct simple_client *client, int tag)
{
   if(!client) return;

   client->tag = TAGMASK(tag);
   print_server_info();
}

void
toggleClientVisible(struct simple_client *client)
{
   if(!client) return;
   
   client->visible ^= 1;

   focus_client(get_top_client_from_output(client->output, false), true);
}

void
toggleClientFixed(struct simple_client *client) 
{
   if(!client) return;
   
   if(client->fixed)
      client->tag = client->output->current_tag;

   client->fixed ^= 1;
}

void
toggleClientFullscreen(struct simple_client *client)
{
   if(!client) return;

   client->fullscreen ^= 1;

   setClientFullscreen(client, client->fullscreen);
}

void setClientFullscreen(struct simple_client *client, int fullscreen)
{
   say(DEBUG, "setClientFullscreen");
   if(!client) return;

   client->fullscreen = fullscreen;
#ifdef XWAYLAND
   if(client->type==XWL_MANAGED_CLIENT || client->type==XWL_UNMANAGED_CLIENT)
      wlr_xwayland_surface_set_fullscreen(client->xwl_surface, client->fullscreen);
   else
#endif
      wlr_xdg_toplevel_set_fullscreen(client->xdg_surface->toplevel, client->fullscreen);

   wlr_scene_node_reparent(&client->scene_tree->node, g_server->layer_tree[client->fullscreen ? LyrFS : LyrClient]);

   if(fullscreen){
      client->prev_geom = client->geom;
      struct wlr_box new_geom;
      new_geom.x = client->output->full_area.x;
      new_geom.y = client->output->full_area.y;
      new_geom.width = client->output->full_area.width;
      new_geom.height = client->output->full_area.height;
   
      client->geom = new_geom;
      set_client_geometry(client);
   } else {
      client->geom = client->prev_geom;
      set_client_geometry(client);
   }
}

void
maximizeClient(struct simple_client *client)
{
   if(!client) return;

   int gap_width = g_config->tile_gap_width;
   int bw = g_config->border_width;

   struct simple_output* output = client->output;

   struct wlr_box new_geom;
   new_geom.x = output->usable_area.x + gap_width + bw;
   new_geom.y = output->usable_area.y + gap_width + bw;
   new_geom.width = output->usable_area.width - gap_width*2 - bw*2;
   new_geom.height = output->usable_area.height - gap_width*2 - bw*2;
   
   client->geom = new_geom;
   set_client_geometry(client);
}

void
killClient(struct simple_client *client)
{
   if(!client) return;

#if XWAYLAND
   if(client->type==XWL_MANAGED_CLIENT || client->type==XWL_UNMANAGED_CLIENT){
      wlr_xwayland_surface_close(client->xwl_surface);
      return;
   }
#endif
   wlr_xdg_toplevel_send_close(client->xdg_surface->toplevel);
}

void
tileClient(struct simple_client *client, enum Direction direction)
{
   say(DEBUG, "tileClient");
   if(!client) return;

   struct simple_output* output = client->output;
   int gap_width = g_config->tile_gap_width;
   int bw = g_config->border_width;

   struct wlr_box new_geom;
   if(direction==LEFT){
      new_geom.x = output->usable_area.x + gap_width + bw;
      new_geom.y = output->usable_area.y + gap_width + bw;
   }
   if(direction==RIGHT){
      new_geom.x = output->usable_area.x + output->usable_area.width/2 + gap_width/2 + bw;
      new_geom.y = output->usable_area.y + gap_width + bw;
   }
   new_geom.width = (output->usable_area.width - (gap_width*3))/2 - bw*2;
   new_geom.height = output->usable_area.height - gap_width*2 - bw*2;

   say(DEBUG, " >> %d %d %d %d", new_geom.x, new_geom.y, new_geom.width, new_geom.height);
   client->geom = new_geom;
   set_client_geometry(client);
}

void
cycleClients(struct simple_output *output){
   say(DEBUG, "cycleClients");

   struct simple_client* client, *selected;
   if(g_server->grabbed_client)
      selected = g_server->grabbed_client;
   else
      selected = get_top_client_from_output(output, true);

   if(!selected) return;

   wl_list_for_each(client, &selected->link, link) {
      if(&client->link == &g_server->clients)
         continue; // wrap past the sentinel node
      if(VISIBLEON(client, output))
         break;
   }

   // grab the client
   g_server->grabbed_client = client;

   // draw the border
   struct client_outline* outline = g_server->grabbed_client_outline;
   if(!outline){
      int line_width = 4;
      outline = client_outline_create(&g_server->scene->tree, g_config->border_colour[OUTLINE], line_width);
      wlr_scene_node_place_above(&outline->tree->node, &g_server->layer_tree[LyrClient]->node);
      g_server->grabbed_client_outline = outline;
   }

   client_outline_set_size(outline, client->geom.width, client->geom.height);
   wlr_scene_node_set_position(&outline->tree->node, client->geom.x, client->geom.y);
   //---
}

void 
begin_interactive(struct simple_client *client, enum CursorMode mode, uint32_t edges)
{
   // this function sets up an interactive move or resize operation
   struct wlr_surface *focused_surface = g_server->seat->pointer_state.focused_surface;
   
   // do not move/request unfocused clients
   if(get_client_surface(client) != wlr_surface_get_root_surface(focused_surface)) 
      return;

   g_server->grabbed_client = client;
   g_server->cursor_mode = mode;

   if(mode == CURSOR_MOVE) {
      //say(DEBUG, "CURSOR_MOVE");
      g_server->grab_x = g_server->cursor->x - client->geom.x;
      g_server->grab_y = g_server->cursor->y - client->geom.y;
      wlr_cursor_set_xcursor(g_server->cursor, g_server->cursor_manager, "fleur");
   } else if(mode == CURSOR_RESIZE) {
      //say(DEBUG, "CURSOR_RESIZE");
      struct wlr_box geo_box;
      if(client->type==XDG_SHELL_CLIENT) {
         wlr_xdg_surface_get_geometry(client->xdg_surface, &geo_box);
         geo_box.x = client->geom.x;
         geo_box.y = client->geom.y;
      } else {
         geo_box = client->geom;
      }

      g_server->grab_x = g_server->cursor->x;
      g_server->grab_y = g_server->cursor->y;
      g_server->grab_box = geo_box;

      g_server->resize_edges = edges;
      wlr_cursor_set_xcursor(g_server->cursor, g_server->cursor_manager, "se-resize");
   }  
}

//------------------------------------------------------------------------
char *
get_client_title(struct simple_client* client)
{
#if XWAYLAND
   if(client->type==XWL_MANAGED_CLIENT || client->type==XWL_UNMANAGED_CLIENT)
      return client->xwl_surface->title;
#endif
   return client->xdg_surface->toplevel->title;
}

char *
get_client_appid(struct simple_client* client)
{
#if XWAYLAND
   if(client->type==XWL_MANAGED_CLIENT || client->type==XWL_UNMANAGED_CLIENT)
      return client->xwl_surface->class;
#endif
   return client->xdg_surface->toplevel->app_id;
}

struct simple_client*
get_top_client_from_output(struct simple_output* output, bool include_hidden)
{
   struct simple_client *client;
   wl_list_for_each(client, &g_server->clients, link) {
      if(!client || &client->link == &g_server->clients) continue; 
      if((include_hidden || client->visible) && VISIBLEON(client, output))
         return client;
   }
   return NULL;
}

int
get_client_at(double lx, double ly, struct simple_client **client, struct wlr_surface **surface, double *sx, double *sy) 
{
  // say(DEBUG, "client_at()");
   struct simple_client* this_client = NULL;
   struct wlr_scene_node* pnode;
   struct wlr_scene_surface *scene_surface = NULL;
   struct wlr_scene_node *node = wlr_scene_node_at(&g_server->scene->tree.node, lx, ly, sx, sy);
   if(node == NULL || node->type != WLR_SCENE_NODE_BUFFER) return -1;

   struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
   scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);

   // go back until the first client
   for(pnode=node; pnode && !this_client; pnode = &pnode->parent->node){
      this_client = pnode->data;
   }

   if(!this_client){
      *surface = scene_surface->surface;
      return -1;
   }

   if(this_client->type == LAYER_SHELL_CLIENT){
      *surface = scene_surface->surface;
      *client = NULL;
      return this_client->type;
   } else {
      *surface = scene_surface->surface;
      *client = this_client;
      return this_client->type;
   }
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
      struct wlr_xwayland_surface *xwl_s = client->xwl_surface;
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

#if XWAYLAND
   struct wlr_xwayland_surface *xs = wlr_xwayland_surface_try_from_wlr_surface(root_surface);
   if (xs){
      //say(DEBUG, "XS");
      *client = xs->data;
      type = (*client)->type;
      return type;
   }
#endif

   struct wlr_layer_surface_v1 *ls = wlr_layer_surface_v1_try_from_wlr_surface(root_surface);
   if(ls) {
      //say(DEBUG, "LAYER");
      if(lsurface) *lsurface = ls->data; 
      return LAYER_SHELL_CLIENT;
   }

   struct wlr_xdg_surface *tmp_s;
   struct wlr_xdg_surface *s = wlr_xdg_surface_try_from_wlr_surface(root_surface);
   while(s) {
      tmp_s = NULL;
      switch(s->role){
         case WLR_XDG_SURFACE_ROLE_POPUP:
            //say(DEBUG, "POPUP");
            if(!s->popup || !s->popup->parent)
               return -1;
            tmp_s = wlr_xdg_surface_try_from_wlr_surface(s->popup->parent);

            if(!tmp_s) {
               return get_client_from_surface(s->popup->parent, client, lsurface);
            }

            s = tmp_s;
            break;
         case WLR_XDG_SURFACE_ROLE_TOPLEVEL:
            //say(DEBUG, "XDG TOPLEVEL");
            *client = s->data;
            if(*client)
               type = (*client)->type;
            return type;
         case WLR_XDG_SURFACE_ROLE_NONE:
            return -1;
      }
   }
   return -1;
}

void
get_client_geometry(struct simple_client *client, struct wlr_box *geom)
{
   if(client->type==XDG_SHELL_CLIENT){
      wlr_xdg_surface_get_geometry(client->xdg_surface, geom);
#if XWAYLAND
   } else {
      geom->x = client->xwl_surface->x;
      geom->y = client->xwl_surface->y;
      geom->width = client->xwl_surface->width;
      geom->height = client->xwl_surface->height;
#endif
   }
}

void
update_border_geometry(struct simple_client *client) {

   struct wlr_box xdg_geom = {0};
   wlr_xdg_surface_get_geometry(client->xdg_surface, &xdg_geom);
   client->geom.width = xdg_geom.width;
   client->geom.height = xdg_geom.height;
   
   //borders
   int bw = g_config->border_width;
   if(client->fullscreen) bw = 0;
   //top
   wlr_scene_rect_set_size(client->border[0], client->geom.width, bw);
   wlr_scene_node_set_position(&client->border[0]->node, 0, -bw);
   //bottom
   wlr_scene_rect_set_size(client->border[1], client->geom.width, bw);
   wlr_scene_node_set_position(&client->border[1]->node, 0, client->geom.height);
   //left
   wlr_scene_rect_set_size(client->border[2], bw, client->geom.height + 2 * bw);
   wlr_scene_node_set_position(&client->border[2]->node, -bw, -bw);
   //right
   wlr_scene_rect_set_size(client->border[3], bw, client->geom.height + 2 * bw);
   wlr_scene_node_set_position(&client->border[3]->node, client->geom.width, -bw);
}

void 
set_client_geometry(struct simple_client *client) 
{
   if(client->type==XDG_SHELL_CLIENT){
      wlr_scene_node_set_position(&client->scene_tree->node, client->geom.x, client->geom.y);
      wlr_scene_node_set_position(&client->scene_surface_tree->node, 0, 0);
      wlr_xdg_toplevel_set_size(client->xdg_surface->toplevel, client->geom.width, client->geom.height);
#if XWAYLAND
   } else {
      wlr_scene_node_set_position(&client->scene_tree->node, client->geom.x, client->geom.y);
      wlr_scene_node_set_position(&client->scene_surface_tree->node, 0, 0);
      wlr_xwayland_surface_configure(client->xwl_surface, 
         client->geom.x, client->geom.y, client->geom.width, client->geom.height);
#endif
   }
   client->resize_requested = 1;
}

void 
set_client_border_colour(struct simple_client *client, int colour) 
{
   float* border_colour = g_config->border_colour[client->fixed ? FIXED : colour];

   for (int i=0; i<4; i++){
      wlr_scene_rect_set_color(client->border[i], border_colour);
   }
}

void 
focus_client(struct simple_client *client, bool raise) 
{
   say(DEBUG, "focus_client()");
   if(!client) return;

   struct wlr_surface *surface = get_client_surface(client); 
   int old_client_type;
   struct simple_client* old_client=NULL;

   if(raise){
      wlr_scene_node_raise_to_top(&client->scene_tree->node);
      if(client->type != XWL_UNMANAGED_CLIENT){
         wl_list_remove(&client->link);
         wl_list_insert(&g_server->clients, &client->link);
#if XWAYLAND
         // restack X11 windows
         if(client->type==XWL_MANAGED_CLIENT)
            wlr_xwayland_surface_restack(client->xwl_surface, NULL, XCB_STACK_MODE_ABOVE);
#endif
      }
   }

   struct wlr_surface *prev_surface = g_server->seat->keyboard_state.focused_surface;
   if(prev_surface==surface) return;

   if(prev_surface){
      old_client_type = get_client_from_surface(prev_surface, &old_client, NULL);
      if(client->type!= XWL_UNMANAGED_CLIENT && (old_client_type == XDG_SHELL_CLIENT || old_client_type == XWL_MANAGED_CLIENT)){
         //deactivate the previously focused surface.
         set_client_activated(old_client, false);
         set_client_border_colour(old_client, UNFOCUSED);
      }
   }
   
   // update the output
   client->output = get_output_at(g_server->cursor->x, g_server->cursor->y);
   g_server->cur_output = client->output;
   
   client->visible = true;
   client->urgent = false;
   set_client_activated(client, true);
   if(client->type != XWL_UNMANAGED_CLIENT)
      set_client_border_colour(client, FOCUSED);
   
   input_focus_surface(surface);

   // only call when raised
   if(raise)
      print_server_info();
}

void 
set_initial_geometry(struct simple_client* client) 
{
   if(wlr_box_empty(&client->geom))
      get_client_geometry(client, &client->geom);

   // Set initial coord based on cursor position
   client->geom.x = g_server->cursor->x;
   client->geom.y = g_server->cursor->y;

   struct simple_output* output = g_server->cur_output;
   struct wlr_box bounds = output->usable_area;

   // check the boundaries
   if(client->geom.x<bounds.x+g_config->border_width) client->geom.x=bounds.x + g_config->border_width;
   if(client->geom.y<bounds.y+g_config->border_width) client->geom.y=bounds.y + g_config->border_width;

   if((client->geom.x+client->geom.width+g_config->border_width) > (bounds.x+bounds.width)) 
      client->geom.x = bounds.x + bounds.width - client->geom.width - g_config->border_width;
   if((client->geom.y+client->geom.height+g_config->border_width) > (bounds.y+bounds.height)) 
      client->geom.y = bounds.y + bounds.height - client->geom.height - g_config->border_width;

   say(DEBUG, " -> Initial geometry : %d %d %d %d", client->geom.x, client->geom.y, client->geom.width, client->geom.height);
   // borders
   for(int i=0; i<4; i++){
      client->border[i] = wlr_scene_rect_create(client->scene_tree, 0, 0, g_config->border_colour[FOCUSED]);
      client->border[i]->node.data = client;
   }

   set_client_geometry(client);
}

// --- Common notify functions -------------------------------------------
static void 
map_notify(struct wl_listener *listener, void *data) 
{
   say(DEBUG, "client_map_notify");
   struct simple_client *client = wl_container_of(listener, client, map);
   struct simple_output* op = g_server->cur_output;

   if(client->visible) return;

   client->scene_tree = wlr_scene_tree_create(g_server->layer_tree[LyrClient]);
   client->scene_tree->node.data = client;

   client->scene_surface_tree = client->type==XDG_SHELL_CLIENT ?
      wlr_scene_xdg_surface_create(client->scene_tree, client->xdg_surface) :
      wlr_scene_subsurface_tree_create(client->scene_tree, get_client_surface(client));
   client->scene_surface_tree->node.data = client;

   if(client->type==XDG_SHELL_CLIENT) {
      client->xdg_surface->surface->data = client->scene_tree;
      client->xdg_surface->data = client;
#if XWAYLAND
   } else {
      client->xwl_surface->surface->data = client->scene_tree;
      client->xwl_surface->data = client;
#endif
   }

   client->output = op;
   client->tag = op->current_tag;
   client->visible = true;
   client->fixed = false;
   client->urgent = false;

#if XWAYLAND
   // Handle unmanaged clients first
   if(client->type==XWL_UNMANAGED_CLIENT){
      get_client_geometry(client, &client->geom);
      wlr_scene_node_reparent(&client->scene_tree->node, g_server->layer_tree[LyrOverlay]);
      wlr_scene_node_set_position(&client->scene_tree->node, client->geom.x, client->geom.y);
      if(wlr_xwayland_or_surface_wants_focus(client->xwl_surface))
         focus_client(client, true);
      return;
   }

   if(client->type==XWL_MANAGED_CLIENT){
      struct wlr_xwayland_surface *xsurface = client->xwl_surface;
      for(int i=0; i<xsurface->window_type_len; i++)
         if(xsurface->window_type[i] == g_server->netatom[NetWMWindowTypeDialog]
            || xsurface->window_type[i] == g_server->netatom[NetWMWindowTypeSplash]
            || xsurface->window_type[i] == g_server->netatom[NetWMWindowTypeToolbar]
            || xsurface->window_type[i] == g_server->netatom[NetWMWindowTypeUtility]){
            say(DEBUG, " >>> XWL POPUP");
            //client->type = XWL_POPUP;
            //wlr_scene_node_reparent(&client->scene_tree->node, server->layer_tree[LyrOverlay]);
         }
   }
#endif
   
   wl_list_insert(&g_server->clients, &client->link);
   set_initial_geometry(client);

   wlr_scene_node_reparent(&client->scene_tree->node, g_server->layer_tree[LyrClient]);

   focus_client(client, true);
}

static void 
unmap_notify(struct wl_listener *listener, void *data) 
{
   say(DEBUG, "client_unmap_notify");
   struct simple_client *client = wl_container_of(listener, client, unmap);

   // reset the cursor mode if the grabbed client was unmapped
   if(client == g_server->grabbed_client) {
      g_server->cursor_mode = CURSOR_NORMAL;
      g_server->grabbed_client = NULL;
   }
   
   client->visible = false;
   client->fixed = false;

#if XWAYLAND
   if(client->type==XWL_UNMANAGED_CLIENT){
      if(client->xwl_surface->surface == g_server->seat->keyboard_state.focused_surface)
         focus_client(get_top_client_from_output(g_server->cur_output, false), true);
   } else
#endif
      wl_list_remove(&client->link);

   if(client->scene_tree)
      wlr_scene_node_destroy(&client->scene_tree->node);
}

static void
commit_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "client_commit_notify");
   struct simple_client *client = wl_container_of(listener, client, commit);

   if(client->xdg_surface->initial_commit){
      wlr_xdg_toplevel_set_wm_capabilities(client->xdg_surface->toplevel, WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN);
      wlr_xdg_toplevel_set_size(client->xdg_surface->toplevel, 0, 0);
      return;
   }
   
   if(client->resize_requested){ 
      update_border_geometry(client);
      client->resize_requested=false;
   }
}

static void 
destroy_notify(struct wl_listener *listener, void *data) 
{
   say(DEBUG, "client_destroy_notify");
   struct simple_client *client = wl_container_of(listener, client, destroy);
//   struct simple_output * output = g_server->cur_output;

   wl_list_remove(&client->destroy.link);
   wl_list_remove(&client->request_fullscreen.link);
   if(client->type==XDG_SHELL_CLIENT){
      wl_list_remove(&client->map.link);
      wl_list_remove(&client->unmap.link);
#if XWAYLAND
   } else {
      wl_list_remove(&client->associate.link);
      wl_list_remove(&client->dissociate.link);
      wl_list_remove(&client->request_activate.link);
      wl_list_remove(&client->request_configure.link);
#endif
   }
   free(client);

   //focus_client(get_top_client_from_output(output, false), true);
}

static void
popup_commit_notify(struct wl_listener *listener, void *data)
{
   struct wlr_surface *surface = data;
   struct wlr_xdg_popup *popup = wlr_xdg_popup_try_from_wlr_surface(surface);

   struct simple_client* client = NULL;
   struct simple_layer_surface *lsurface = NULL;

   struct wlr_box box;

   if (!popup->base->initial_commit) return;

   int type = get_client_from_surface(popup->base->surface, &client, &lsurface);
   if(!popup->parent || type<0) return;

   struct wlr_scene_tree *tree = wlr_scene_xdg_surface_create(popup->parent->data, popup->base);
   popup->base->surface->data = tree;
   box = type == LAYER_SHELL_CLIENT ? lsurface->output->usable_area : client->output->usable_area;
   box.x -= (type==LAYER_SHELL_CLIENT ? lsurface->geom.x : client->geom.x); 
   box.y -= (type==LAYER_SHELL_CLIENT ? lsurface->geom.y : client->geom.y);
   wlr_xdg_popup_unconstrain_from_box(popup, &box);

   wl_list_remove(&listener->link);
}

static void
fullscreen_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "fullscreen_notify");
   struct simple_client *client = wl_container_of(listener, client, request_fullscreen);
   int want_fullscreen = 0;
#ifdef XWAYLAND
   if(client->type==XWL_MANAGED_CLIENT || client->type==XWL_UNMANAGED_CLIENT)
      want_fullscreen = client->xwl_surface->fullscreen;
   else
#endif
      want_fullscreen = client->xdg_surface->toplevel->requested.fullscreen;
   
   say(DEBUG, "want_fullscreen = %u", want_fullscreen);

   setClientFullscreen(client, want_fullscreen);
}

// --- XDG Shell ---------------------------------------------------------
void 
xdg_new_toplevel_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "new_xdg_surface_notify");
   struct wlr_xdg_toplevel *xdg_toplevel = data;

   // allocate a simple_client for this surface
   struct simple_client *xdg_client = calloc(1, sizeof(struct simple_client));
   xdg_client->type = XDG_SHELL_CLIENT;
   xdg_client->xdg_surface = xdg_toplevel->base;

   LISTEN(&xdg_toplevel->events.destroy, &xdg_client->destroy, destroy_notify);
   LISTEN(&xdg_toplevel->base->surface->events.map, &xdg_client->map, map_notify);
   LISTEN(&xdg_toplevel->base->surface->events.unmap, &xdg_client->unmap, unmap_notify);
   LISTEN(&xdg_toplevel->base->surface->events.commit, &xdg_client->commit, commit_notify);
   LISTEN(&xdg_toplevel->events.request_fullscreen, &xdg_client->request_fullscreen, fullscreen_notify);
}

static struct wl_listener popup_commit_listener;

void
xdg_new_popup_notify(struct wl_listener *listener, void *data)
{
   //void
   struct wlr_xdg_popup *xdg_popup = data;

   LISTEN(&xdg_popup->base->surface->events.commit, &popup_commit_listener, &popup_commit_notify);
}

//---- XWayland Shell ----------------------------------------------------
#if XWAYLAND
static void 
xwl_associate_notify(struct wl_listener *listener, void *data) 
{
   say(DEBUG, "xwl_associate_notify");
   struct simple_client *xwl_client = wl_container_of(listener, xwl_client, associate);

   LISTEN(&xwl_client->xwl_surface->surface->events.map, &xwl_client->map, map_notify);
   LISTEN(&xwl_client->xwl_surface->surface->events.unmap, &xwl_client->unmap, unmap_notify);
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
   say(DEBUG, "xwl_request_activate_notify");
   struct simple_client *client = wl_container_of(listener, client, request_activate);
   if(client->type==XWL_MANAGED_CLIENT)
      wlr_xwayland_surface_activate(client->xwl_surface, 1);
}

static void 
xwl_request_configure_notify(struct wl_listener *listener, void *data) 
{
   say(DEBUG, "xwl_request_configure_notify");
   struct simple_client *client = wl_container_of(listener, client, request_configure);
   struct wlr_xwayland_surface_configure_event *event = data;

   if(!wlr_box_empty(&client->geom)){
      client->geom.x = event->x;          client->geom.y = event->y;
      client->geom.width = event->width;  client->geom.height = event->height;
      set_client_geometry(client);
   }

   wlr_xwayland_surface_configure(client->xwl_surface, event->x, event->y, event->width, event->height);
}

static void
xwl_set_hints_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "xwl_set_hints_notify");
}

static void
xwl_set_title_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "xwl_set_title_notify");
}

/*
static void
xwl_request_fullscreen_notify(struct wl_listener *listener, void *data)
{
   say(DEBUG, "xwl_request_fullscreen_notify");
}
*/

xcb_atom_t
getatom(xcb_connection_t *xc, const char *name)
{
   xcb_atom_t atom = 0;
   xcb_intern_atom_reply_t *reply;
   xcb_intern_atom_cookie_t cookie = xcb_intern_atom(xc, 0, strlen(name), name);
   if ((reply = xcb_intern_atom_reply(xc, cookie, NULL)))
      atom = reply->atom;
   free(reply);
   return atom;
}

//------------------------------------------------------------------------
void 
xwl_ready_notify(struct wl_listener *listener, void *data) 
{
   say(DEBUG, "xwl_ready_notify");

   xcb_connection_t *xc = xcb_connect(g_server->xwayland->display_name, NULL);
   int err = xcb_connection_has_error(xc);
   if(err)
      say(WARNING, "xcb_connect to X server failed with code %d", err);

   g_server->netatom[NetWMWindowTypeDialog] = getatom(xc, "_NET_WM_WINDOW_TYPE_DIALOG");
   g_server->netatom[NetWMWindowTypeSplash] = getatom(xc, "_NET_WM_WINDOW_TYPE_SPLASH");
   g_server->netatom[NetWMWindowTypeToolbar] = getatom(xc, "_NET_WM_WINDOW_TYPE_TOOLBAR");
   g_server->netatom[NetWMWindowTypeUtility] = getatom(xc, "_NET_WM_WINDOW_TYPE_UTILITY");

   wlr_xwayland_set_seat(g_server->xwayland, g_server->seat);
   
   struct wlr_xcursor *xcursor;
   xcursor = wlr_xcursor_manager_get_xcursor(g_server->cursor_manager, "left_ptr", 1);
   if(xcursor){
      struct wlr_xcursor_image *image = xcursor->images[0];
      wlr_xwayland_set_cursor(g_server->xwayland, image->buffer, 
            image->width*4, image->width, image->height, image->hotspot_x, image->hotspot_y);
   }

   xcb_disconnect(xc);
}

void 
xwl_new_surface_notify(struct wl_listener *listener, void *data) 
{
   say(DEBUG, "xwayland_new_surface_notify");
   struct wlr_xwayland_surface *xsurface = data;

   // Create simple_client for this surface 
   struct simple_client *xwl_client = calloc(1, sizeof(struct simple_client));
   xwl_client->type = xsurface->override_redirect ? XWL_UNMANAGED_CLIENT : XWL_MANAGED_CLIENT;
   xwl_client->xwl_surface = xsurface;

   LISTEN(&xsurface->events.associate, &xwl_client->associate, xwl_associate_notify);
   LISTEN(&xsurface->events.dissociate, &xwl_client->dissociate, xwl_dissociate_notify);
   LISTEN(&xsurface->events.destroy, &xwl_client->destroy, destroy_notify);

   LISTEN(&xsurface->events.request_activate, &xwl_client->request_activate, xwl_request_activate_notify);
   LISTEN(&xsurface->events.request_configure, &xwl_client->request_configure, xwl_request_configure_notify);
   LISTEN(&xsurface->events.request_fullscreen, &xwl_client->request_fullscreen, fullscreen_notify);

   LISTEN(&xsurface->events.set_hints, &xwl_client->set_hints, xwl_set_hints_notify);
   LISTEN(&xsurface->events.set_title, &xwl_client->set_title, xwl_set_title_notify);
   // More mappings
}
#endif

