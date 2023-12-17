#ifndef IPC_H
#define IPC_H

struct DwlIpcOutput {
   struct wl_list link;
   struct wl_resource *resource;
   struct simple_output* output;
};

void dwl_ipc_manager_bind(struct wl_client*, void*, uint32_t, uint32_t);

void dwl_ipc_output_printstatus(struct simple_output*);

#endif
