#ifndef IPC_H
#define IPC_H

struct simple_ipc_output {
   struct wl_list link;
   struct wl_resource *resource;
   struct simple_output* output;
};

void ipc_manager_bind(struct wl_client*, void*, uint32_t, uint32_t);

void ipc_output_printstatus(struct simple_output*);

#endif
