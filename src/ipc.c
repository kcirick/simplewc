#include <wlr/backend/session.h>
#include <wlr/types/wlr_cursor.h>
#include <xkbcommon/xkbcommon.h>

#include "dwl-ipc-unstable-v2-protocol.h"
#include "globals.h"
#include "layer.h"
#include "client.h"
#include "server.h"
#include "ipc.h"

static void dwl_ipc_manager_destroy(struct wl_resource *resource);
static void dwl_ipc_manager_get_output(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *output);
static void dwl_ipc_manager_release(struct wl_client *client, struct wl_resource *resource);
static void dwl_ipc_output_destroy(struct wl_resource *resource);
static void dwl_ipc_output_printstatus_to(struct DwlIpcOutput*);
static void dwl_ipc_output_set_client_tags(struct wl_client *client, struct wl_resource *resource, uint32_t and_tags, uint32_t xor_tags);
static void dwl_ipc_output_set_layout(struct wl_client *client, struct wl_resource *resource, uint32_t index);
static void dwl_ipc_output_set_tags(struct wl_client *client, struct wl_resource *resource, uint32_t tagmask, uint32_t toggle_tagset);
static void dwl_ipc_output_release(struct wl_client *client, struct wl_resource *resource);


static struct zdwl_ipc_manager_v2_interface dwl_manager_implementation = {
   .release = dwl_ipc_manager_release,
   .get_output = dwl_ipc_manager_get_output
};

static struct zdwl_ipc_output_v2_interface dwl_output_implementation = {
   .release = dwl_ipc_output_release,
   .set_tags = dwl_ipc_output_set_tags,
   .set_layout = dwl_ipc_output_set_layout,
   .set_client_tags = dwl_ipc_output_set_client_tags
};

void
dwl_ipc_manager_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_resource *manager_resource = wl_resource_create(client, &zdwl_ipc_manager_v2_interface, version, id);
	if (!manager_resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(manager_resource, &dwl_manager_implementation, NULL, dwl_ipc_manager_destroy);

	zdwl_ipc_manager_v2_send_tags(manager_resource, MAX_TAGS);

	//for (int i = 0; i < LENGTH(layouts); i++)
	//	zdwl_ipc_manager_v2_send_layout(manager_resource, layouts[i].symbol);
}

void
dwl_ipc_manager_destroy(struct wl_resource *resource)
{
	/* No state to destroy */
}

void
dwl_ipc_manager_get_output(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *output)
{
	struct DwlIpcOutput *ipc_output;
	struct simple_output *sop = wlr_output_from_resource(output)->data;
   //struct simple_server *server = sop->server;
	struct wl_resource *output_resource = wl_resource_create(client, &zdwl_ipc_output_v2_interface, wl_resource_get_version(resource), id);
	if (!output_resource)
		return;

	ipc_output = calloc(1, sizeof(*ipc_output));
	ipc_output->resource = output_resource;
 	ipc_output->output = sop;
	wl_resource_set_implementation(output_resource, &dwl_output_implementation, ipc_output, dwl_ipc_output_destroy);
	
   wl_list_insert(&sop->dwl_ipc_outputs, &ipc_output->link);
	dwl_ipc_output_printstatus_to(ipc_output);
}

void
dwl_ipc_manager_release(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
dwl_ipc_output_destroy(struct wl_resource *resource)
{
	struct DwlIpcOutput *ipc_output = wl_resource_get_user_data(resource);
	wl_list_remove(&ipc_output->link);
	free(ipc_output);
}

void
dwl_ipc_output_printstatus(struct simple_output *output)
{
	struct DwlIpcOutput *ipc_output;
	wl_list_for_each(ipc_output, &output->dwl_ipc_outputs, link)
		dwl_ipc_output_printstatus_to(ipc_output);
}

void
dwl_ipc_output_printstatus_to(struct DwlIpcOutput *ipc_output)
{
	struct simple_output *output = ipc_output->output;
   struct simple_server *server = output->server;
	struct simple_client *c, *focused;
	int tagmask, state, numclients, focused_client, tag;
   //const char *title, *appid;
	
   focused = get_top_client_from_output(output);
	zdwl_ipc_output_v2_send_active(ipc_output->resource, output == server->cur_output);

   ///////////////////////////////////////////
   for (tag = 0 ; tag < server->config->n_tags; tag++) {
      numclients = state = focused_client = 0;
		tagmask = 1 << tag;
		if ((tagmask & output->visible_tags) != 0)
			state |= ZDWL_IPC_OUTPUT_V2_TAG_STATE_ACTIVE;

		wl_list_for_each(c, &server->clients, link) {
			if (c->output != output)
				continue;
			if (!(c->tag & tagmask))
				continue;
			if (c == focused)
				focused_client = 1;
			//if (c->isurgent)
			//	state |= ZDWL_IPC_OUTPUT_V2_TAG_STATE_URGENT;

			numclients++;
		}
		zdwl_ipc_output_v2_send_tag(ipc_output->resource, tag, state, numclients, focused_client);
	}
	//title = focused ? client_get_title(focused) : "";
	//appid = focused ? client_get_appid(focused) : "";
   ////////////////////////////////////////////////

	//zdwl_ipc_output_v2_send_layout(ipc_output->resource, monitor->lt[monitor->sellt] - layouts);
	//zdwl_ipc_output_v2_send_title(ipc_output->resource, title ? title : broken);
	//zdwl_ipc_output_v2_send_appid(ipc_output->resource, appid ? appid : broken);
	//zdwl_ipc_output_v2_send_layout_symbol(ipc_output->resource, monitor->ltsymbol);
	//if (wl_resource_get_version(ipc_output->resource) >= ZDWL_IPC_OUTPUT_V2_FULLSCREEN_SINCE_VERSION) {
	//	zdwl_ipc_output_v2_send_fullscreen(ipc_output->resource, focused ? focused->isfullscreen : 0);
	//}
	//if (wl_resource_get_version(ipc_output->resource) >= ZDWL_IPC_OUTPUT_V2_FLOATING_SINCE_VERSION) {
	//	zdwl_ipc_output_v2_send_floating(ipc_output->resource, focused ? focused->isfloating : 0);
	//}
	zdwl_ipc_output_v2_send_frame(ipc_output->resource);
}

void
dwl_ipc_output_set_client_tags(struct wl_client *client, struct wl_resource *resource, uint32_t and_tags, uint32_t xor_tags)
{
	struct DwlIpcOutput *ipc_output;
	struct simple_output *output;
	struct simple_client *selected_client;
	unsigned int newtags = 0;

	ipc_output = wl_resource_get_user_data(resource);
	if (!ipc_output)
		return;

	output = ipc_output->output;
   struct simple_server* server = output->server;
	selected_client = get_top_client_from_output(output);
	if (!selected_client)
		return;

	newtags = (selected_client->tag & and_tags) ^ xor_tags;
	if (!newtags)
		return;

	selected_client->tag = newtags;
	//focusclient(get_top_client_from_output(server->cur_output), 1);
	arrange_output(server->cur_output);
	print_server_info(server);
}

void
dwl_ipc_output_set_layout(struct wl_client *client, struct wl_resource *resource, uint32_t index)
{
   /*
	struct DwlIpcOutput *ipc_output;
	struct simple_output *output;

	ipc_output = wl_resource_get_user_data(resource);
	if (!ipc_output)
		return;

	output = ipc_output->output;
	if (index >= LENGTH(layouts))
		return;
	if (index != monitor->lt[monitor->sellt] - layouts)
		monitor->sellt ^= 1;

	monitor->lt[monitor->sellt] = &layouts[index];
	arrange_output(output);
	printstatus();
   */
}

void
dwl_ipc_output_set_tags(struct wl_client *client, struct wl_resource *resource, uint32_t tagmask, uint32_t toggle_tagset)
{
	struct DwlIpcOutput *ipc_output;
	struct simple_output *output;
	//unsigned int newtags = tagmask & TAGMASK;
	unsigned int newtags = tagmask;

	ipc_output = wl_resource_get_user_data(resource);
	if (!ipc_output)
		return;
	output = ipc_output->output;
   struct simple_server *server = output->server;

	//if (!newtags || newtags == output->tagset[monitor->seltags])
	if (!newtags || newtags == output->visible_tags)
		return;
	//if (toggle_tagset)
	//	monitor->seltags ^= 1;

	//output->tagset[monitor->seltags] = newtags;
	output->visible_tags = newtags;
   if(toggle_tagset) output->current_tag = newtags;
	//focusclient(focustop(monitor), 1);
	arrange_output(output);
	print_server_info(server);
}

void
dwl_ipc_output_release(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

