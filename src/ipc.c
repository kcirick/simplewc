#include <string.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_cursor.h>

#include "dwl-ipc-unstable-v2-protocol.h"
#include "globals.h"
#include "layer.h"
#include "client.h"
#include "server.h"
#include "output.h"
#include "action.h"
#include "ipc.h"

static void ipc_manager_release(struct wl_client *, struct wl_resource *);
static void ipc_manager_get_output(struct wl_client *, struct wl_resource *, uint32_t, struct wl_resource *);
static void ipc_manager_send_action(struct wl_client *, struct wl_resource *, const char*);

static void ipc_output_printstatus_to(struct simple_ipc_output*);
static void ipc_output_release(struct wl_client *, struct wl_resource *);
static void ipc_output_set_client_tags(struct wl_client *, struct wl_resource *, uint32_t, uint32_t);
static void ipc_output_set_tags(struct wl_client *, struct wl_resource *, uint32_t, uint32_t);

static struct zdwl_ipc_manager_v2_interface ipc_manager_implementation = {
   .release = ipc_manager_release,
   .get_output = ipc_manager_get_output,
   .send_action = ipc_manager_send_action
};

static struct zdwl_ipc_output_v2_interface ipc_output_implementation = {
   .release = ipc_output_release,
   .set_tags = ipc_output_set_tags,
   .set_client_tags = ipc_output_set_client_tags,
};

//--- Public functions ---------------------------------------------------
static void
ipc_manager_destroy(struct wl_resource *resource)
{
	/* No state to destroy */
}

void
ipc_manager_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_resource *manager_resource = wl_resource_create(client, &zdwl_ipc_manager_v2_interface, version, id);
	if (!manager_resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(manager_resource, &ipc_manager_implementation, NULL, ipc_manager_destroy);

	zdwl_ipc_manager_v2_send_tags(manager_resource, g_config->n_tags);
}

void
ipc_output_printstatus(struct simple_output *output)
{
	struct simple_ipc_output *ipc_output;
	wl_list_for_each(ipc_output, &output->ipc_outputs, link)
		ipc_output_printstatus_to(ipc_output);
}

//--- IPC manager implementation -----------------------------------------
static void
ipc_output_destroy(struct wl_resource *resource)
{
	struct simple_ipc_output *ipc_output = wl_resource_get_user_data(resource);
	wl_list_remove(&ipc_output->link);
	free(ipc_output);
}

void
ipc_manager_release(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

void
ipc_manager_get_output(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *output)
{
	struct simple_ipc_output *ipc_output;
	struct simple_output *sop = wlr_output_from_resource(output)->data;

	struct wl_resource *output_resource = wl_resource_create(client, &zdwl_ipc_output_v2_interface, wl_resource_get_version(resource), id);
	if (!output_resource)
		return;

	ipc_output = calloc(1, sizeof(*ipc_output));
	ipc_output->resource = output_resource;
 	ipc_output->output = sop;
	wl_resource_set_implementation(output_resource, &ipc_output_implementation, ipc_output, ipc_output_destroy);
	
   wl_list_insert(&sop->ipc_outputs, &ipc_output->link);
	ipc_output_printstatus_to(ipc_output);
}

void
ipc_manager_send_action(struct wl_client *client, struct wl_resource *resource, const char* action)
{
   say(INFO, "ipc_output_send_action: %s", action);
   process_ipc_action(action);
}

//--- IPC output implementation ------------------------------------------
void
ipc_output_printstatus_to(struct simple_ipc_output *ipc_output)
{
	struct simple_output *output = ipc_output->output;
	struct simple_client *c, *focused;
	int tagmask, state, numclients, focused_client, tag;
   char *title, *appid;
	
   focused = get_top_client_from_output(output, false);
	zdwl_ipc_output_v2_send_active(ipc_output->resource, output == g_server->cur_output);

   ///////////////////////////////////////////
   for (tag = 0 ; tag < g_config->n_tags; tag++) {
      numclients = state = focused_client = 0;
		tagmask = 1 << tag;
		if ((tagmask & output->visible_tags) != 0)
			state |= ZDWL_IPC_OUTPUT_V2_TAG_STATE_ACTIVE;

		wl_list_for_each(c, &g_server->clients, link) {
			if (c->output != output)
				continue;
			if (!(c->tag & tagmask))
				continue;
			if (c == focused)
				focused_client = 1;
			if (c->urgent)
				state |= ZDWL_IPC_OUTPUT_V2_TAG_STATE_URGENT;

			numclients++;
		}
		zdwl_ipc_output_v2_send_tag(ipc_output->resource, tag, state, numclients, focused_client);
	}
	title = focused ? get_client_title(focused) : "";
	appid = focused ? get_client_appid(focused) : "";
   ////////////////////////////////////////////////

	zdwl_ipc_output_v2_send_title(ipc_output->resource, title ? title : "broken");
	zdwl_ipc_output_v2_send_appid(ipc_output->resource, appid ? appid : "broken");
	//if (wl_resource_get_version(ipc_output->resource) >= ZDWL_IPC_OUTPUT_V2_FULLSCREEN_SINCE_VERSION) {
	//	zdwl_ipc_output_v2_send_fullscreen(ipc_output->resource, focused ? focused->isfullscreen : 0);
	//}
	//if (wl_resource_get_version(ipc_output->resource) >= ZDWL_IPC_OUTPUT_V2_FLOATING_SINCE_VERSION) {
	//	zdwl_ipc_output_v2_send_floating(ipc_output->resource, focused ? focused->isfloating : 0);
	//}
	zdwl_ipc_output_v2_send_frame(ipc_output->resource);
}

void
ipc_output_release(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

//--- IPC output set implementation --------------------------------------
void
ipc_output_set_client_tags(struct wl_client *client, struct wl_resource *resource, uint32_t and_tags, uint32_t xor_tags)
{
	struct simple_ipc_output *ipc_output;
	struct simple_output *output;
	struct simple_client *selected_client;
	uint32_t newtags = 0;

	ipc_output = wl_resource_get_user_data(resource);
	if (!ipc_output) return;

	output = ipc_output->output;
	selected_client = get_top_client_from_output(output, false);
	if (!selected_client) return;

	newtags = (selected_client->tag & and_tags) ^ xor_tags;
	if (!newtags) return;

	selected_client->tag = newtags;
	arrange_output(g_server->cur_output);
	print_server_info();
}

void
ipc_output_set_tags(struct wl_client *client, struct wl_resource *resource, uint32_t tagmask, uint32_t toggle_tagset)
{
	struct simple_ipc_output *ipc_output;
	struct simple_output *output;
	unsigned int newtags = tagmask;

	ipc_output = wl_resource_get_user_data(resource);
	if (!ipc_output) return;

	output = ipc_output->output;

	if (!newtags || newtags == output->visible_tags)
		return;
	//if (toggle_tagset)
	//	monitor->seltags ^= 1;

	output->visible_tags = newtags;
   if(toggle_tagset) output->current_tag = newtags;
	arrange_output(output);
	print_server_info();
}

