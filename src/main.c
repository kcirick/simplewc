/*
 * main.c
 *   - Main SimpleWC Program
 */

#include <getopt.h>
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
   char buffer[256];
   va_list args;
   va_start(args, message);
   vsnprintf(buffer, 256, message, args);
   va_end(args);

   wlr_log(level==DEBUG?WLR_DEBUG:WLR_INFO, CRESET"[%s]: %s", msg_str[level], buffer);

   if(level==ERROR) exit(EXIT_FAILURE);
}

void
spawn(char* cmd)
{
   say(DEBUG, "Spawn %s", cmd);

   char *sh = NULL;
   if(!(sh=getenv("SHELL"))) sh = (char*)"/bin/sh";

   // from dwl:
   pid_t pid = fork();
   if(pid==0) {
      dup2(STDERR_FILENO, STDOUT_FILENO);
      setsid();
      execl(sh, sh, "-c", cmd, (char*)NULL);
   }
}

void
signal_handler(int sig)
{
   if(sig == SIGCHLD) {
#if XWAYLAND
      siginfo_t in;
      while ( !waitid(P_ALL, 0, &in, WEXITED|WNOHANG|WNOWAIT)
              && in.si_pid
              && (!g_server->xwayland || in.si_pid != g_server->xwayland->server->pid) )
         waitpid(in.si_pid, NULL, 0);
#else
      while (0 < waitpid(-1, NULL, WNOHANG));
#endif
   } else if (sig == SIGINT || sig == SIGTERM)
      wl_display_terminate(g_server->display);
}

void
exit_simplewc() 
{
   char* pid_c = getenv("SIMPLEWC_PID");
   if(!pid_c) say(ERROR, "SIMPLEWC_PID not set");
   
   int pid_i = atoi(pid_c);
   if(!pid_i) say(ERROR, "pid should not be zero");
   
   kill(pid_i, SIGTERM);
}

//--- Main function ------------------------------------------------------
int
main(int argc, char **argv)
{
   char config_file[64] = { '\0' };
   char start_cmd[64] = { '\0' };

   // Parse arguments
   int opt, long_index;
   static struct option long_options[] = {
      { "config",    required_argument,   0, 'c' },
      { "start",     required_argument,   0, 's' },
      { "exit",      no_argument,         0, 'e' },
      { "info",      no_argument,         0, 'i' },
      { "debug",     no_argument,         0, 'd' },
      { "version",   no_argument,         0, 'v' },
      { "help",      no_argument,         0, 'h' },
      { 0, 0, 0, 0 }
    };
   while ((opt = getopt_long(argc, argv,"c:s:divh", long_options, &long_index )) != -1) {
      switch (opt) {
         case 'c' :
            sprintf(config_file, optarg);
            break;
         case 's' :
            sprintf(start_cmd, optarg);
            break;
         case 'e' :
            exit_simplewc();
            exit(EXIT_SUCCESS);
         case 'd' :
            info_level = WLR_DEBUG;
            break;
         case 'i' :
            info_level = WLR_INFO;
            break;
         case 'v' :
            printf("simplewc v"VERSION"\n");
            exit(EXIT_SUCCESS);
         default:
            printf("Usage: simplewc [--config file][--start cmd][--exit][--debug|--info][--version][--help]\n");
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
   sa.sa_flags = 0;
   sa.sa_handler = signal_handler;
   sigemptyset(&sa.sa_mask);

   for(int i=0; i<LENGTH(signals); i++)
      sigaction(signals[i], &sa, NULL);

   // Start WLR logging
   wlr_log_init(info_level, NULL);

   // set SIMPLEWC_PID environment variable
   char pid[32];
   snprintf(pid, sizeof(pid), "%d", getpid());
   if(setenv("SIMPLEWC_PID", pid, true)<0)
      say(ERROR, "unable to set SIMPLEWC_PID");
   else
      say(INFO, "SIMPLEWC_PID=%s", pid);

   // Read in config
   if(!(g_config = calloc(1, sizeof(struct simple_config))))
      say(ERROR, "Cannot allocate g_config");
   readConfiguration(config_file);

   // Create a server
   if(!(g_server = calloc(1, sizeof(struct simple_server))))
      say(ERROR, "Cannot allocate g_server");
   prepareServer();

   startServer();

   // Run autostarts and startup comand if defined
   if(start_cmd[0]!='\0')                    spawn(start_cmd);
   if(g_config->autostart_script[0]!='\0')   spawn(g_config->autostart_script);

   // Run the main Wayland event loop
   wl_display_run(g_server->display);

   cleanupServer();

   return EXIT_SUCCESS;
}
