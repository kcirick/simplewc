#include <wayland-server-core.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/box.h>
#include <xkbcommon/xkbcommon.h>

#include "globals.h"
#include "layer.h"
#include "client.h"
#include "server.h"

void 
key_function(struct simple_server *server, struct keymap *keymap) 
{
   //--- QUIT -----
   if(keymap->keyfn==QUIT)    wl_display_terminate(server->display);

   //--- SPAWN -----
   if(keymap->keyfn==SPAWN)   spawn(keymap->argument);

   //--- TAG -----
   if(keymap->keyfn==TAG){
      if(!strcmp(keymap->argument, "prev"))     setCurrentTag(server, /*curtag*/-1);
      if(!strcmp(keymap->argument, "next"))     setCurrentTag(server, /*curtag*/+1);
      if(!strcmp(keymap->argument, "select"))   setCurrentTag(server, keymap->keysym-XKB_KEY_1);
   }

   //--- CLIENT -----
   struct wlr_surface *surface = server->seat->keyboard_state.focused_surface;
   struct simple_client* client;
   get_client_from_surface(surface, &client, NULL);
   if(client && keymap->keyfn==CLIENT) {
      if(!strcmp(keymap->argument, "toggle_tag"))  toggleClientTag(client, keymap->keysym-XKB_KEY_1);
      if(!strcmp(keymap->argument, "cycle"))       cycleClients(client->output);
      if(!strcmp(keymap->argument, "move")){
         say(DEBUG, " >>> move client");
         if(keymap->keysym==XKB_KEY_Left)    client->geom.x-=server->config->moveresize_step;
         if(keymap->keysym==XKB_KEY_Right)   client->geom.x+=server->config->moveresize_step;
         if(keymap->keysym==XKB_KEY_Up)      client->geom.y-=server->config->moveresize_step;
         if(keymap->keysym==XKB_KEY_Down)    client->geom.y+=server->config->moveresize_step;
         set_client_geometry(client, client->geom);
      }
      if(!strcmp(keymap->argument, "resize")){
         say(DEBUG, " >>> resize client");
         if(keymap->keysym==XKB_KEY_Left)    client->geom.width-=server->config->moveresize_step;
         if(keymap->keysym==XKB_KEY_Right)   client->geom.width+=server->config->moveresize_step;
         if(keymap->keysym==XKB_KEY_Up)      client->geom.height-=server->config->moveresize_step;
         if(keymap->keysym==XKB_KEY_Down)    client->geom.height+=server->config->moveresize_step;
         set_client_geometry(client, client->geom);
      }
      // ...
      arrange_output(server->cur_output);
   }
}

void 
mouse_function(struct simple_client *client, struct mousemap *mousemap)
{
   say(DEBUG, "mouse_function");
   /*
   if(context==CONTEXT_ROOT){
      if(argument=="test") test();
   }
   if(context==CONTEXT_CLIENT){
      if(!client) return;
      if(argument=="move") client->dragMove();
      if(argument=="resize") client->dragResize();
   }
   */
}
