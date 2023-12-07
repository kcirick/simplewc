#ifndef XWAYLAND_H
#define XWAYLAND_H

void xwayland_new_surface_notify(struct wl_listener*, void *);
void xwayland_ready_notify(struct wl_listener*, void *);

void initializeXWayland(struct simple_server*);
void startXWayland(struct simple_server*);

#endif
