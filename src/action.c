#include <wayland-server-core.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include <xkbcommon/xkbcommon.h>

#include "globals.h"
#include "server.h"

void 
key_function(struct simple_server *server, struct keymap *keymap) 
{
   //--- QUIT -----
   if(keymap->keyfn==QUIT)    wl_display_terminate(server->display);

   //--- SPAWN -----
   if(keymap->keyfn==SPAWN)   spawn(keymap->argument);
}
