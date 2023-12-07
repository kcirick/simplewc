#include <unistd.h>
#include <ctype.h>
#include <wayland-server-core.h>
#include <xkbcommon/xkbcommon.h>
#include <wlr/types/wlr_keyboard.h>

#include "globals.h"

void colour2rgba(const char *color, float dest[static 4]) {
   if(color[0] == '#') ++color;

   int len = strlen(color);
   if(len!=6 || !isxdigit(color[0]) || !isxdigit(color[1])) return;

   char *ptr;
   uint32_t parsed_colour = strtoul(color, &ptr, 16);
   if (*ptr != '\0') return;

   dest[0] = ((parsed_colour >> 16) & 0xff) / 255.0;
   dest[1] = ((parsed_colour >> 8) & 0xff) / 255.0;
   dest[2] = (parsed_colour  & 0xff) / 255.0;
   dest[3] = 255.0 / 255.0;
}

void trim(char *orig) {
   size_t i, len;

   if(orig==NULL) return;
   
   // right trim
   len = strlen(orig);
   if(len == 0) return;
   for (i=len-1; i>0; i--){
      if(isspace(orig[i])) orig[i]=0;
      else                 break;
   }
   if(isspace(orig[i])) orig[i]=0;

   // left trim
   i=0;
   len = strlen(orig);
   if(len==0) return;
   while(orig[i] && isspace(orig[i])) i++;
   memmove(orig, orig+i, len -i + 1);
}

//------------------------------------------------------------------------
void set_defaults(struct simple_config *config){
   config->n_tags = 4;
   config->border_width = 2;
   config->sloppy_focus = false;
   config->moveresize_step = 10;

   colour2rgba("#111111", config->background_colour);
   colour2rgba("#0000FF", config->border_colour[FOCUSED]);
   colour2rgba("#CCCCCC", config->border_colour[UNFOCUSED]);
   colour2rgba("#FF0000", config->border_colour[URGENT]);
   colour2rgba("#00FF00", config->border_colour[MARKED]);
   colour2rgba("#0000FF", config->border_colour[FIXED]);
}

void readConfiguration(struct simple_config* config, char* filename) {
   say(INFO, "Reading configuration file %s", filename);

   set_defaults(config);

   wl_list_init(&config->key_bindings);
   //wl_list_init(&config->mouse_bindings);

   FILE *f;
   if(!(f=fopen(filename, "r")))
      say(ERROR, "Error reading file %s", filename);

   char buffer[64];
   char id[32];
   char value[64];
   char* token;
   while (fgets(buffer, sizeof buffer, f)){
      if(buffer[0]=='\n' || buffer[0]=='#') continue;
      
      token = strtok(buffer, "=");
      strncpy(id, token, sizeof id); 
      trim(id);
      //if(id==NULL) continue;

      token=strtok(NULL, "=");
      strncpy(value, token, sizeof value);
      trim(value);
      //if(value==NULL) continue;

      say(DEBUG, "config id = '%s' / value = '%s'", id, value);
      if(!strcmp(id, "n_tags")) config->n_tags=atoi(value);
      if(!strcmp(id, "tag_names")){
         token = strtok(value, ";");
         strncpy(config->tag_names[0], token, sizeof config->tag_names[0]);
         trim(config->tag_names[0]);
         for(int i=1; i<config->n_tags; i++){
            token = strtok(NULL, ";");
            strncpy(config->tag_names[i], token, sizeof config->tag_names[i]);
            trim(config->tag_names[i]);
         }
      }

      if(!strcmp(id, "border_width")) config->border_width = atoi(value);
      if(!strcmp(id, "moveresize_step")) config->moveresize_step = atoi(value);
      
      if(!strcmp(id, "background_colour")) 
         colour2rgba(value, config->background_colour);
      if(!strcmp(id, "border_colour_focus")) 
         colour2rgba(value, config->border_colour[FOCUSED]);
      if(!strcmp(id, "border_colour_unfocus")) 
         colour2rgba(value, config->border_colour[UNFOCUSED]);
      if(!strcmp(id, "border_colour_urgent")) 
         colour2rgba(value, config->border_colour[URGENT]);
      if(!strcmp(id, "border_colour_marked")) 
         colour2rgba(value, config->border_colour[MARKED]);
      if(!strcmp(id, "border_colour_fixed")) 
         colour2rgba(value, config->border_colour[FIXED]);

      if(!strcmp(id, "KEY")){
         char binding[32];
         token = strtok(value, " ");
         strncpy(binding, token, sizeof binding);
         trim(binding);
         
         char function[32];
         token = strtok(NULL, " ");
         strncpy(function, token, sizeof function);
         trim(function);

         char args[32];
         token = strtok(NULL, "");
         strncpy(args, token, sizeof args);
         trim(args);

         uint32_t mod = 0;
         xkb_keysym_t keysym;
         char keys[16];
         token = strtok(binding, "+");
         strncpy(keys, token, sizeof keys);
         bool do_continue = true;
         while(do_continue){
                 if(!strcmp(keys, "S"))   mod |= WLR_MODIFIER_SHIFT;
            else if(!strcmp(keys, "C"))   mod |= WLR_MODIFIER_CTRL;
            else if(!strcmp(keys, "A"))   mod |= WLR_MODIFIER_ALT;
            else if(!strcmp(keys, "W"))   mod |= WLR_MODIFIER_LOGO;
            else {
               keysym = xkb_keysym_from_name(keys, XKB_KEYSYM_CASE_INSENSITIVE);
               do_continue = false;
            }
            token = strtok(NULL, "+");
            if(token) strncpy(keys, token, sizeof keys);
         }

         int this_fn = -1;
              if(!strcmp(function, "QUIT"))     this_fn = QUIT;
         else if(!strcmp(function, "TAG"))      this_fn = TAG;
         else if(!strcmp(function, "SPAWN"))    this_fn = SPAWN;
         else if(!strcmp(function, "CLIENT"))   this_fn = CLIENT;
         
         struct keymap *keybind = calloc(1, sizeof(struct keymap));
         keybind->mask = mod;
         keybind->keysym = keysym;
         keybind->keyfn = this_fn;
         strncpy(keybind->argument, args, sizeof keybind->argument);

         wl_list_insert(&config->key_bindings, &keybind->link);
      }

      if(!strcmp(id, "MOUSE")){
         // TODO
      }
   }

}
