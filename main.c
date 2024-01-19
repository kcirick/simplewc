/*
 * main.c
 *   - Main SimpleWC Program
 */

#include <signal.h>
#include <sys/wait.h>
#include <wlr/util/log.h>

#include "globals.h"
#include "server.h"

#define CRED      "\033[31m"
#define CGREEN    "\033[32m"
#define CYELLOW   "\033[33m"
#define CBLUE     "\033[34m"
#define CPURPLE   "\033[35m"
#define CRESET    "\033[0m"

static const char *msg_str[NMSG] = { CBLUE"DEBUG"CRESET, "INFO", CYELLOW"WARNING"CRESET, CRED"ERROR"CRESET };
static int info_level = WLR_SILENT;

struct wlr_session *g_session;
struct simple_server* g_server;
struct simple_config* g_config;

//------------------------------------------------------------------------
void 
say(int level, const char* message, ...) 
{
   if(level==DEBUG && info_level!=WLR_DEBUG) return;

   char buffer[256];
   va_list args;
   va_start(args, message);
   vsnprintf(buffer, 256, message, args);
   va_end(args);

   printf("SimpleWC [%s]: %s\n", msg_str[level], buffer);
   //wlr_log(WLR_INFO, " [%s]: %s", msg_str[level], buffer);

   if(level==ERROR) exit(EXIT_FAILURE);
}

void 
spawn(char* cmd) 
{
   char *sh = NULL;
   if(!(sh=getenv("SHELL"))) sh = (char*)"/bin/sh";

   say(DEBUG, "Spawn %s", cmd);
   pid_t pid = fork();
   if(pid==0){
      pid_t child;
      setsid();
      sigset_t set;
      sigemptyset(&set);
      sigprocmask(SIG_SETMASK, &set, NULL);
      if((child=fork())==0){
         execl(sh, sh, "-c", cmd, (char*) NULL);
         exit(0);
      }
      exit(0);
   }
   //waitpid(pid, NULL, 0);
}

void 
signal_handler(int sig) 
{
   if(sig == SIGCHLD) {
#if XWAYLAND
      siginfo_t in;
      while (  !waitid(P_ALL, 0, &in, WEXITED|WNOHANG|WNOWAIT) 
               && in.si_pid
               && (!g_server->xwayland || in.si_pid != g_server->xwayland->server->pid) )
         waitpid(in.si_pid, NULL, 0);
#else
      while (0 < waitpid(-1, NULL, WNOHANG));
#endif
   } else if (sig == SIGINT || sig == SIGTERM )
      wl_display_terminate(g_server->display);
}

//--- Main function ------------------------------------------------------
int 
main(int argc, char **argv) 
{
   char config_file[64] = { '\0' };
   char start_cmd[64] = { '\0' };

   // Parse arguments
   for(int i=1; i<argc; i++){
      char* iarg = argv[i];
      if(!strcmp(iarg, "--config") && ((i+1)<argc)) {
         sprintf(config_file, argv[++i]);
      }
      else if (!strcmp(iarg, "--start") && ((i+1)<argc)) {
         sprintf(start_cmd, argv[++i]);
      }
      else if(!strcmp(iarg, "--debug")) {
         info_level = WLR_DEBUG;
      }
      else if(!strcmp(iarg, "--version")) {
         say(INFO, "Version-"VERSION);
         exit(EXIT_SUCCESS);
      }
      else if(!strcmp(iarg, "--help")) {
         say(INFO, "Usage: %s [--config file][--start cmd][--debug][--version][--help]", argv[0]);
         exit(EXIT_SUCCESS);
      }
   }
   if(config_file[0]=='\0')
      sprintf(config_file, "%s/%s", getenv("HOME"), ".config/simplewc/configrc");

   // Wayland requires XDG_RUNTIME_DIR for creating its communications socket
   if(!getenv("XDG_RUNTIME_DIR"))
      say(ERROR, "XDG_RUNTIME_DIR must be set!");

   // Handle signals
   int signals[] = { SIGCHLD, SIGINT, SIGTERM, SIGPIPE };
   struct sigaction sa;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = 0;
   sa.sa_handler = signal_handler;

   for(int i=0; i<LENGTH(signals); i++)
      sigaction(signals[i], &sa, NULL);

   // Start WLR logging
   wlr_log_init(info_level, NULL);

   // Read in config
   if(!(g_config = calloc(1, sizeof(struct simple_config))))
      say(ERROR, "Cannot allocate g_config");
   readConfiguration(config_file);

   // Create a server
   if(!(g_server = calloc(1, sizeof(struct simple_server))))
      say(ERROR, "Cannot allocate g_server");
   prepareServer();
   
   startServer(start_cmd);

   // Run the main Wayland event loop
   wl_display_run(g_server->display);
   
   cleanupServer();
     
   return EXIT_SUCCESS;
}
