#include <ctype.h>
#include <string.h>
#include <linux/input-event-codes.h>
#include <wlr/types/wlr_keyboard.h>

#include "globals.h"

void 
colour2rgba(const char *color, float dest[static 4]) 
{
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

void 
trim(char *orig) 
{
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
void 
set_defaults()
{
   g_config->n_tags = 4;
   g_config->border_width = 2;
   g_config->focus_type = 0;
   g_config->moveresize_step = 10;
   g_config->new_client_placement = HYBRID;

   colour2rgba("#111111", g_config->background_colour);
   colour2rgba("#0000FF", g_config->border_colour[FOCUSED]);
   colour2rgba("#CCCCCC", g_config->border_colour[UNFOCUSED]);
   colour2rgba("#FF0000", g_config->border_colour[URGENT]);
   colour2rgba("#00FF00", g_config->border_colour[MARKED]);
   colour2rgba("#0000FF", g_config->border_colour[FIXED]);
   colour2rgba("#FFFFFF", g_config->border_colour[OUTLINE]);

   g_config->autostart_script[0] = '\0';
   g_config->xkb_layout[0] = '\0';
   g_config->xkb_options[0] = '\0';

   g_config->tablet_rotation = 0;
   g_config->tablet_boundary_x[0] = 0; g_config->tablet_boundary_x[1] = 1.;
   g_config->tablet_boundary_y[0] = 0; g_config->tablet_boundary_y[1] = 1.;
}

void
readConfiguration(char* filename) 
{
   say(INFO, "Reading configuration file %s", filename);
   if(strcmp(g_config->config_file_name, filename))
      strncpy(g_config->config_file_name, filename, sizeof g_config->config_file_name);

   set_defaults();

   wl_list_init(&g_config->key_bindings);
   wl_list_init(&g_config->mouse_bindings);

   FILE *f;
   if(!(f=fopen(g_config->config_file_name, "r"))){
      say(WARNING, "Error reading file %s", g_config->config_file_name);
      return;
   }

   char buffer[128];
   char id[32];
   char value[128];
   char* token;
   while (fgets(buffer, sizeof buffer, f)){
      if(buffer[0]=='\n' || buffer[0]=='#') continue;
      
      token = strtok(buffer, "=");
      strncpy(id, token, sizeof id); 
      trim(id);

      token=strtok(NULL, "=");
      strncpy(value, token, sizeof value);
      trim(value);

      say(DEBUG, "config id = '%s' / value = '%s'", id, value);
         
      if(!strcmp(id, "n_tags")) g_config->n_tags=atoi(value);

      if(!strcmp(id, "border_width"))     g_config->border_width = atoi(value);
      if(!strcmp(id, "tile_gap_width"))   g_config->tile_gap_width = atoi(value);
      if(!strcmp(id, "moveresize_step"))  g_config->moveresize_step = atoi(value);
      if(!strcmp(id, "focus_type"))       g_config->focus_type = atoi(value);
      if(!strcmp(id, "touchpad_tap_click"))  g_config->touchpad_tap_click = !strcmp(value, "true") ? true : false; 
      if(!strcmp(id, "new_client_placement")) g_config->new_client_placement = atoi(value);

      if(!strcmp(id, "background_colour"))      colour2rgba(value, g_config->background_colour);
      if(!strcmp(id, "border_colour_focus"))    colour2rgba(value, g_config->border_colour[FOCUSED]);
      if(!strcmp(id, "border_colour_unfocus"))  colour2rgba(value, g_config->border_colour[UNFOCUSED]);
      if(!strcmp(id, "border_colour_urgent"))   colour2rgba(value, g_config->border_colour[URGENT]);
      if(!strcmp(id, "border_colour_marked"))   colour2rgba(value, g_config->border_colour[MARKED]);
      if(!strcmp(id, "border_colour_fixed"))    colour2rgba(value, g_config->border_colour[FIXED]);
      if(!strcmp(id, "border_colour_outline"))  colour2rgba(value, g_config->border_colour[OUTLINE]);

      if(!strcmp(id, "autostart"))     strncpy(g_config->autostart_script, value, sizeof g_config->autostart_script);

      if(!strcmp(id, "xkb_layout"))    strncpy(g_config->xkb_layout, value, sizeof g_config->xkb_layout);
      if(!strcmp(id, "xkb_options"))   strncpy(g_config->xkb_options, value, sizeof g_config->xkb_options);

      if(!strcmp(id, "tablet_rotation"))  g_config->tablet_rotation = atoi(value);
      if(!strcmp(id, "tablet_boundary_x")){
         token = strtok(value, " ");
         g_config->tablet_boundary_x[0] = atof(token);

         token = strtok(NULL, " ");
         g_config->tablet_boundary_x[1] = atof(token);
      }
      if(!strcmp(id, "tablet_boundary_y")){
         token = strtok(value, " ");
         g_config->tablet_boundary_y[0] = atof(token);

         token = strtok(NULL, " ");
         g_config->tablet_boundary_y[1] = atof(token);
      }
      if(!strcmp(id, "KEY")){
         char binding[32];
         token = strtok(value, " ");
         strncpy(binding, token, sizeof binding);
         trim(binding);
         
         char function[32];
         token = strtok(NULL, " ");
         strncpy(function, token, sizeof function);
         trim(function);

         char args[64];
         token = strtok(NULL, "");
         strncpy(args, token, sizeof args);
         trim(args);

         uint32_t mod = 0;
         xkb_keysym_t keysym;
         char keys[32];
         token = strtok(binding, "+");
         strncpy(keys, token, sizeof keys);
         bool do_continue = true;
         while(do_continue){
                 if(!strcmp(keys, "S"))   mod |= WLR_MODIFIER_SHIFT;
            else if(!strcmp(keys, "C"))   mod |= WLR_MODIFIER_CTRL;
            else if(!strcmp(keys, "A"))   mod |= WLR_MODIFIER_ALT;
            else if(!strcmp(keys, "W"))   mod |= WLR_MODIFIER_LOGO;
            else {
               keysym = xkb_keysym_from_name(keys, XKB_KEYSYM_NO_FLAGS);
               do_continue = false;
            }
            token = strtok(NULL, "+");
            if(token) strncpy(keys, token, sizeof keys);
         }

         int this_fn = -1;
              if(!strcmp(function, "QUIT"))     this_fn = QUIT;
         else if(!strcmp(function, "LOCK"))     this_fn = LOCK;
         else if(!strcmp(function, "TAG"))      this_fn = TAG;
         else if(!strcmp(function, "SPAWN"))    this_fn = SPAWN;
         else if(!strcmp(function, "CLIENT"))   this_fn = CLIENT;
         
         struct keymap *keybind = calloc(1, sizeof(struct keymap));
         keybind->mask = mod;
         keybind->keysym = keysym;
         keybind->keyfn = this_fn;
         strncpy(keybind->argument, args, sizeof keybind->argument);

         wl_list_insert(&g_config->key_bindings, &keybind->link);
      }

      if(!strcmp(id, "MOUSE")){
         char binding[32];
         token = strtok(value, " ");
         strncpy(binding, token, sizeof binding);
         trim(binding);
         
         char context[32];
         token = strtok(NULL, " ");
         strncpy(context, token, sizeof context);
         trim(context);

         char args[64];
         token = strtok(NULL, "");
         strncpy(args, token, sizeof args);
         trim(args);

         unsigned int mod = 0;
         unsigned int button = 0;
         char button_char[32];
         token = strtok(binding, "+");
         strncpy(button_char, token, sizeof button_char);
         bool do_continue = true;
         while(do_continue) {
                 if(!strcmp(button_char, "S"))  mod |= WLR_MODIFIER_SHIFT;
            else if(!strcmp(button_char, "C"))  mod |= WLR_MODIFIER_CTRL;
            else if(!strcmp(button_char, "A"))  mod |= WLR_MODIFIER_ALT;
            else if(!strcmp(button_char, "W"))  mod |= WLR_MODIFIER_LOGO;
            else {
                    if(!strcmp(button_char, "Button_Left"))  button = BTN_LEFT;
               else if(!strcmp(button_char, "Button_Right"))  button = BTN_RIGHT;
               else if(!strcmp(button_char, "Button_Middle"))  button = BTN_MIDDLE;

               do_continue = false;
            }

            token = strtok(NULL, "+");
            if(token)   strncpy(button_char, token, sizeof button_char);
         }

         int this_context = -1;
              if(!strcmp(context, "ROOT"))   this_context = CONTEXT_ROOT;
         else if(!strcmp(context, "CLIENT")) this_context = CONTEXT_CLIENT;

         struct mousemap *mousebind = calloc(1, sizeof(struct mousemap));
         mousebind->mask = mod;
         mousebind->button = button;
         mousebind->context = this_context;
         strncpy(mousebind->argument, args, sizeof mousebind->argument);

         wl_list_insert(&g_config->mouse_bindings, &mousebind->link);
      }
   }
   fclose(f);
}
/*
void
reloadConfiguration() {
   if(g_config->config_file_name[0]=='\0')
      say(ERROR, "config file %s could no longer be found!", g_config->config_file_name);

   readConfiguration(g_config->config_file_name);
}
*/
