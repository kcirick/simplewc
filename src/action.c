#include <signal.h>
#include <string.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/box.h>

#include "globals.h"
#include "layer.h"
#include "client.h"
#include "server.h"
#include "output.h"

void 
key_function(struct keymap *keymap) 
{
   //--- QUIT -----
   if(keymap->keyfn==QUIT)    wl_display_terminate(g_server->display);
   
   //--- SPAWN -----
   if(keymap->keyfn==SPAWN)   spawn(keymap->argument);

   //--- TAG -----
   if(keymap->keyfn==TAG){
      if(!strcmp(keymap->argument, "prev"))     setCurrentTag(/*curtag*/-1, false);
      if(!strcmp(keymap->argument, "next"))     setCurrentTag(/*curtag*/+1, false);
      if(!strcmp(keymap->argument, "select"))   setCurrentTag(keymap->keysym-XKB_KEY_1, false);
      if(!strcmp(keymap->argument, "toggle"))   setCurrentTag(keymap->keysym-XKB_KEY_1, true);
      if(!strcmp(keymap->argument, "tile"))     tileTag();

      arrange_output(g_server->cur_output);
   }

   //--- CLIENT -----
   struct wlr_surface *surface = g_server->seat->keyboard_state.focused_surface;
   struct simple_client* client = NULL;
   int type = get_client_from_surface(surface, &client, NULL);

   if(keymap->keyfn==CLIENT) {
      if(!strcmp(keymap->argument, "cycle"))          cycleClients(g_server->cur_output);

      if(type<0) return;
      if(!strcmp(keymap->argument, "send_to_tag"))    sendClientToTag(client, keymap->keysym-XKB_KEY_1);
      if(!strcmp(keymap->argument, "toggle_fixed"))   toggleClientFixed(client);
      if(!strcmp(keymap->argument, "toggle_visible")) toggleClientVisible(client);
      if(!strcmp(keymap->argument, "toggle_fullscreen")) toggleClientFullscreen(client);
      if(!strcmp(keymap->argument, "toggle_maximize"))   toggleClientMaximize(client);
      if(!strcmp(keymap->argument, "kill"))           killClient(client);
      if(!strcmp(keymap->argument, "tile_left"))      tileClient(client, LEFT);
      if(!strcmp(keymap->argument, "tile_right"))     tileClient(client, RIGHT);
      if(!strcmp(keymap->argument, "move")){
         if(keymap->keysym==XKB_KEY_Left)    client->geom.x-=g_config->moveresize_step;
         if(keymap->keysym==XKB_KEY_Right)   client->geom.x+=g_config->moveresize_step;
         if(keymap->keysym==XKB_KEY_Up)      client->geom.y-=g_config->moveresize_step;
         if(keymap->keysym==XKB_KEY_Down)    client->geom.y+=g_config->moveresize_step;
         set_client_geometry(client);
      }
      if(!strcmp(keymap->argument, "resize")){
         if(keymap->keysym==XKB_KEY_Left)    client->geom.width-=g_config->moveresize_step;
         if(keymap->keysym==XKB_KEY_Right)   client->geom.width+=g_config->moveresize_step;
         if(keymap->keysym==XKB_KEY_Up)      client->geom.height-=g_config->moveresize_step;
         if(keymap->keysym==XKB_KEY_Down)    client->geom.height+=g_config->moveresize_step;
         set_client_geometry(client);
      }
      // ...
      arrange_output(g_server->cur_output);
   }
}

void 
mouse_function(struct simple_client *client, struct mousemap *mousemap, int resize_edges)
{
   if(mousemap->context==CONTEXT_ROOT){
      if(!strcmp(mousemap->argument, "test"))   say(INFO, "test()");
   }
   if(mousemap->context==CONTEXT_CLIENT){
      if(!client) return;
      if(!strcmp(mousemap->argument, "move"))   begin_interactive(client, CURSOR_MOVE, 0);
      if(!strcmp(mousemap->argument, "resize")) begin_interactive(client, CURSOR_RESIZE, resize_edges);
   }
}

void
process_ipc_action(const char* action)
{
   if(!strcmp(action, "test"))      say(INFO, "Action test");
   if(!strcmp(action, "quit"))      wl_display_terminate(g_server->display);
}
