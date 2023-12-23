/*
 * swc-msg
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include <wayland-client.h>
#include <wayland-util.h>

#include "dwl-ipc-unstable-v2-protocol.h"


enum MessageType     { DEBUG, INFO, WARNING, ERROR, NMSG };
enum Mode            { NOMODE = 0, SET = 1<<0, GET = 1<<1, WATCH = 1<<2 | GET };

static const char *msg_str[NMSG] = { "DEBUG", "INFO", "WARNING", "ERROR" };

static struct wl_display *display;
static struct zdwl_ipc_manager_v2 *ipc_manager;

static enum Mode mode = NOMODE;
bool flag_tagcount;
bool flag_tag;
bool flag_output;
bool flag_client;

struct output {
   char *output_name;
   uint32_t name;
};
static int outputs_buflen = 4;
static struct output *outputs;
static int outputcount;

static char *output_name;
static int tagcount;
static uint32_t vis_tags;
static uint32_t focused_tag;
static char arg[64] = { '\0' };

static void noop() {}

//------------------------------------------------------------------------
void 
say(int level, const char* message, ...) 
{
   //if(level==DEBUG && info_level!=WLR_DEBUG) return;

   char buffer[256];
   va_list args;
   va_start(args, message);
   vsnprintf(buffer, 256, message, args);
   va_end(args);

   printf("SWC-MSG [%s]: %s", msg_str[level], buffer);

   if(level==ERROR) exit(EXIT_FAILURE);
}

//------------------------------------------------------------------------
static void simple_ipc_tags(void *, struct zdwl_ipc_manager_v2 *, uint32_t);
static const struct zdwl_ipc_manager_v2_listener ipc_listener = {
   .tags = simple_ipc_tags,
   .layout = noop,
};

void
simple_ipc_tags(void *data, struct zdwl_ipc_manager_v2 *ipc_manager, uint32_t count)
{
   tagcount = count;
   if(mode&GET && flag_tagcount)
      say(INFO, "Server: tagcount = %d\n", tagcount);
}

//------------------------------------------------------------------------
static void simple_ipc_output_active(void *, struct zdwl_ipc_output_v2 *, uint32_t);
static void simple_ipc_output_tag(void *, struct zdwl_ipc_output_v2 *, uint32_t, uint32_t, uint32_t, uint32_t);
static void simple_ipc_output_title(void *, struct zdwl_ipc_output_v2 *, const char*);
static void simple_ipc_output_appid(void *, struct zdwl_ipc_output_v2 *, const char*);
static void simple_ipc_output_fullscreen(void *, struct zdwl_ipc_output_v2 *, uint32_t);
static void simple_ipc_output_frame(void *, struct zdwl_ipc_output_v2 *);
static const struct zdwl_ipc_output_v2_listener ipc_output_listener = {
   .active = simple_ipc_output_active,
   .tag = simple_ipc_output_tag,
   .layout = noop,
   .title = simple_ipc_output_title,
   .appid = simple_ipc_output_appid,
   .layout_symbol = noop,
   .fullscreen = simple_ipc_output_fullscreen,
   .floating = noop,
   .frame = simple_ipc_output_frame,
};

void
simple_ipc_output_active(void *data, struct zdwl_ipc_output_v2 *dwl_ipc_output, uint32_t active)
{
   //say(INFO, "dwl_ipc_output_active\n"); 
   if(!(mode&GET && flag_output)) return;

   char* output_name = data;
   say(INFO, " |--> active = %u\n", active?1:0);
}

void
simple_ipc_output_tag(void *data, struct zdwl_ipc_output_v2 *dwl_ipc_output, 
      uint32_t tag, uint32_t state, uint32_t clients, uint32_t focused)
{
   //say(INFO, "dwl_ipc_output_tag\n"); 
   if(state == ZDWL_IPC_OUTPUT_V2_TAG_STATE_ACTIVE) vis_tags |= 1 << tag;
   if(focused) focused_tag = 1 << tag;

   if(!flag_tag) return;

   if(mode&GET) {
      say(INFO, " |--> tag %u\n", tag);
      say(INFO, "   |--> active: %u / n clients = %u / focused client: %u\n", state, clients, focused);
   }
}

void
simple_ipc_output_title(void *data, struct zdwl_ipc_output_v2 *dwl_ipc_output, const char* title)
{
   //say(INFO, "dwl_ipc_output_title\n"); 
   if(!(mode&GET && flag_client && (!strcmp(arg, "title")||!strcmp(arg, "all")))) return;
   
   say(INFO, " |--> focused client title: %s\n", title);
}

void
simple_ipc_output_appid(void *data, struct zdwl_ipc_output_v2 *dwl_ipc_output, const char* appid)
{
   //say(INFO, "dwl_ipc_output_appid\n"); 
   if(!(mode&GET && flag_client && (!strcmp(arg, "appid")||!strcmp(arg, "all")))) return;
   say(INFO, " |--> focused client appid: %s\n", appid);
}

void
simple_ipc_output_fullscreen(void *data, struct zdwl_ipc_output_v2 *dwl_ipc_output, uint32_t is_fullscreen)
{
   //say(INFO, "dwl_ipc_output_fullscreen\n"); 
   if(!(mode&GET && flag_client && (!strcmp(arg, "fullscreen")||!strcmp(arg, "all")))) return;
   char* output_name = data;
   if(output_name) say(INFO, "%s ", output_name);
   printf("fullscreen %u\n", is_fullscreen);
}

void
simple_ipc_output_frame(void *data, struct zdwl_ipc_output_v2 *dwl_ipc_output)
{
   if(mode!=SET) return;

   if(flag_client) {
      uint32_t new_tag = (1<<(atoi(arg)-1));
      zdwl_ipc_output_v2_set_client_tags(dwl_ipc_output, ~focused_tag, new_tag);
   }
   if(flag_tag) {
      char *t = arg;
      uint32_t mask = vis_tags;
      int i=0;

      for(; *t; t++, i++){
         switch (*t) {
            case '-':
               mask &=~(1<<i);
               break;
            case '+':
               mask |= 1<<i;
               break;
            case '^':
               mask ^= 1<<i;
               break;
         }
      }
      if (i>tagcount) say(ERROR, "bad tagset %s\n", arg);

      say(INFO, "mask = %u\n", mask);
      zdwl_ipc_output_v2_set_tags(dwl_ipc_output, mask, 0);
   }
   wl_display_flush(display);
}

//------------------------------------------------------------------------
static void simple_output_name(void *, struct wl_output *, const char *);
static const struct wl_output_listener output_listener = {
   .geometry = noop,
   .mode = noop,
   .done = noop,
   .scale = noop,
   .name = simple_output_name,
   .description = noop,
};

void
simple_output_name(void *data, struct wl_output *output, const char *name)
{
   if(outputs) {
      struct output *o = (struct output*) data;
      o->output_name = strdup(name);
      say(INFO, "Output + %s\n", name);
   } else
      say(INFO, "Output: %s\n", name);
   if(output_name && !strcmp(output_name, name)) {
      wl_output_release(output);
      return;
   }

   struct zdwl_ipc_output_v2 *dwl_ipc_output = zdwl_ipc_manager_v2_get_output(ipc_manager, output);
   zdwl_ipc_output_v2_add_listener(dwl_ipc_output, &ipc_output_listener, output_name ? NULL : strdup(name));
}

//------------------------------------------------------------------------
static void global_add(void *, struct wl_registry *, uint32_t, const char *, uint32_t);
static void global_remove(void *, struct wl_registry *, uint32_t);
static const struct wl_registry_listener registry_listener = {
   .global = global_add,
   .global_remove = global_remove,
};

void
global_add(void *data, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version)
{
   if(!strcmp(interface, wl_output_interface.name)) {
      struct wl_output *o = wl_registry_bind(wl_registry, name, &wl_output_interface, WL_OUTPUT_NAME_SINCE_VERSION);

      wl_output_add_listener(o, &output_listener, outputs ? &outputs[outputcount] : NULL);
      if(!outputs) return;
      
      if(outputcount > outputs_buflen) {
         outputs_buflen *= 2;
         outputs = realloc(outputs, outputs_buflen * sizeof(struct output));
      }
      outputs[outputcount].name = name;
      outputcount++;
   } else if (!strcmp(interface, zdwl_ipc_manager_v2_interface.name)) {
      ipc_manager = wl_registry_bind(wl_registry, name, &zdwl_ipc_manager_v2_interface, 2);
      zdwl_ipc_manager_v2_add_listener(ipc_manager, &ipc_listener, NULL);
   }
}

void
global_remove(void *data, struct wl_registry *wl_registry, uint32_t name)
{
   if(!outputs) return;
   for(int i=0; i<outputcount; i++) {
      if(outputs[i].name == name){
         say(INFO, "- %s\n", outputs[i].output_name);
         free(outputs[i].output_name);
         outputs[i] = outputs[--outputcount];
      }
   }
}

//--- Main function ------------------------------------------------------
int 
main(int argc, char **argv) 
{
   // Parse arguments
   // first pass - get the mode
   for(int i=1; i<argc; i++){
      char* iarg = argv[i];
      if(!strcmp(iarg, "--set"))    mode = SET;
      if(!strcmp(iarg, "--get"))    mode = GET;
      if(!strcmp(iarg, "--watch"))  mode = WATCH;
   }
   if(mode==NOMODE) say(ERROR, "No mode selected!\n");
   
   // second pass - get the arguments
   for(int i=1; i<argc; i++){
      char* iarg = argv[i];
      if(mode==SET){
         if(!strcmp(iarg, "--client") && ((i+1)<argc)){
            flag_client = true;
            sprintf(arg, argv[++i]);
         }
         if(!strcmp(iarg, "--tag") && ((i+1)<argc)){
            flag_tag = true;
            sprintf(arg, argv[++i]);
         }
      } else if(mode==GET || mode==WATCH) {
         if(!strcmp(iarg, "--tagcount")){
            flag_tagcount = true;
         }
         if(!strcmp(iarg, "--output")){
            flag_output = true;
         }
         if(!strcmp(iarg, "--client")){
            flag_client = true;
            sprintf(arg, ((i+1)<argc) ? argv[++i]:"all");
         }
         if(!strcmp(iarg, "--tag")){
            flag_tag = true;
         }
      }
   }
   if(mode&GET && !(flag_tagcount || flag_tag || flag_output || flag_client)){
      say(INFO, "all flags\n");
      sprintf(arg, "all");
      flag_tagcount = flag_tag = flag_output = flag_client = 1;
   }

   display = wl_display_connect(NULL);
   if(!display) say(ERROR, "Bad display\n");

   struct wl_registry *registry = wl_display_get_registry(display);
   wl_registry_add_listener(registry, &registry_listener, NULL);

   wl_display_dispatch(display);
   wl_display_roundtrip(display);

   if(!ipc_manager)
      say(ERROR, "Bad IPC protocol\n");

   wl_display_roundtrip(display);

   if(mode==WATCH)
      while(wl_display_dispatch(display) != -1);

   return EXIT_SUCCESS;
}
