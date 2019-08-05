/*
* Copyright © 2019 Samsung Electronics co., Ltd. All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice (including the next
* paragraph) shall be included in all copies or substantial portions of the
* Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <pepper.h>
#include <wayland-server.h>
#include <wayland-util.h>
#include <pepper-inotify.h>
#include <pepper-keyrouter.h>
#include <pepper-xkb.h>
#include <headless_server.h>

#define MAX_CMDS	256

#define STDOUT_REDIR			"stdout"
#define STDERR_REDIR			"stderr"
#define PROTOCOL_TRACE_ON		"protocol_trace_on"
#define PROTOCOL_TRACE_OFF		"protocol_trace_off"
#define KEYGRAB_STATUS			"keygrab_status"
#define TOPVWINS			"topvwins"
#define CONNECTED_CLIENTS		"connected_clients"
#define CLIENT_RESOURCES		"reslist"
#define KEYMAP				"keymap"
#define HELP_MSG			"help"

typedef struct
{
	pepper_compositor_t *compositor;
	pepper_inotify_t *inotify;

	pepper_view_t *top_mapped;
	pepper_view_t *focus;
} headless_debug_t;

typedef void (*headless_debug_action_cb_t)(headless_debug_t *hd, void *data);

typedef struct
{
   const char *cmds;
   headless_debug_action_cb_t cb;
   headless_debug_action_cb_t disable_cb;
} headless_debug_action_t;

const static int KEY_DEBUG = 0xdeaddeb0;
extern void wl_debug_server_enable(int enable);

static void
_headless_debug_usage()
{
	fprintf(stdout, "Supported commands:\n\n");
	fprintf(stdout, "\t %s\n", PROTOCOL_TRACE_ON);
	fprintf(stdout, "\t %s\n", PROTOCOL_TRACE_OFF);
	fprintf(stdout, "\t %s\n", STDOUT_REDIR);
	fprintf(stdout, "\t %s\n", STDERR_REDIR);
	fprintf(stdout, "\t %s\n", KEYGRAB_STATUS);
	fprintf(stdout, "\t %s\n", TOPVWINS);
	fprintf(stdout, "\t %s\n", CONNECTED_CLIENTS);
	fprintf(stdout, "\t %s\n", CLIENT_RESOURCES);
	fprintf(stdout, "\t %s\n", KEYMAP);
	fprintf(stdout, "\t %s\n", HELP_MSG);

	fprintf(stdout, "\nTo execute commands, just create/remove/update a file with the commands above.\n");
	fprintf(stdout, "Please refer to the following examples.\n\n");
	fprintf(stdout, "\t # winfo protocol_trace_on\t : enable event trace\n");
	fprintf(stdout, "\t # winfo event_trace_off\t : disable event trace\n");
	fprintf(stdout, "\t # winfo stdout\t\t\t : redirect STDOUT\n");
	fprintf(stdout, "\t # winfo stderr\t\t\t : redirect STDERR\n");
	fprintf(stdout, "\t # winfo keygrab_status\t\t : display keygrab status\n");
	fprintf(stdout, "\t # winfo topvwins\t\t : display top/visible window stack\n");
	fprintf(stdout, "\t # winfo connected_clients\t : display connected clients information\n");
	fprintf(stdout, "\t # winfo reslist\t\t : display each resources information of connected clients\n");
	fprintf(stdout, "\t # winfo keymap\t\t : display current xkb keymap\n");
	fprintf(stdout, "\t # winfo help\t\t\t : display this help message\n");
}

static void
_headless_debug_protocol_trace_on(headless_debug_t *hdebug, void *data)
{
	(void) hdebug;
	(void) data;
	wl_debug_server_enable(1);
}

static void
_headless_debug_protocol_trace_off(headless_debug_t *hdebug, void *data)
{
	(void) hdebug;
	(void) data;
	wl_debug_server_enable(0);
}

static void
_headless_debug_dummy(headless_debug_t *hdebug, void *data)
{
	(void) hdebug;
	(void) data;
	_headless_debug_usage();
}

static enum wl_iterator_result
_client_get_resource_itr(struct wl_resource *resource, void *data)
{
	int *n_resources = (int *)data;

	PEPPER_TRACE("\t\t [resource][%d] class=%s, id=%u\n", ++(*n_resources), wl_resource_get_class(resource), wl_resource_get_id(resource));

	return WL_ITERATOR_CONTINUE;
}

static void
_headless_debug_connected_clients(headless_debug_t *hdebug, void *data)
{
	pid_t pid;
	uid_t uid;
	gid_t gid;

	int client_fd = -1;
	uint32_t n_clients = 0;
	int n_resources = 0;

	struct wl_list *clist = NULL;
	struct wl_client *client_itr = NULL;
	pepper_bool_t need_reslist = PEPPER_FALSE;

	const char *cmds = (const char *)data;

	/* check if reslist feature is required */
	if (cmds && !strncmp(cmds, CLIENT_RESOURCES, MAX_CMDS)) {
		need_reslist = PEPPER_TRUE;
	}

	/* get client list which bound wl_compositor global */
	clist = wl_display_get_client_list(pepper_compositor_get_display(hdebug->compositor));
	PEPPER_CHECK(clist, return, "Failed to get client list from compositor->display.\n");

	PEPPER_TRACE("========= [Connected clients information] =========\n");
	wl_client_for_each(client_itr, clist) {
		n_clients++;
		client_fd = wl_client_get_fd(client_itr);
		wl_client_get_credentials(client_itr, &pid, &uid, &gid);
		PEPPER_TRACE("\t client[%d]: pid=%d, user=%d, group=%d, socket_fd=%d", n_clients, pid, uid, gid, client_fd);

		if (PEPPER_FALSE == need_reslist) {
			PEPPER_TRACE("\n");
			continue;
		}

		PEPPER_TRACE("\n");
		wl_client_for_each_resource(client_itr, _client_get_resource_itr, &n_resources);
		PEPPER_TRACE("\t\t number of resources = %d\n", n_resources);

		n_resources = 0;
	}

	if (!n_clients)
		PEPPER_TRACE("============ [No connected clients] ===========\n\n");
}

static void
_headless_debug_redir_stdout(headless_debug_t *hdebug, void *data)
{
	(void) hdebug;
	(void) data;

	int fd = -1;
	int ret = 0;

	fd = open("/run/pepper/stdout.txt", O_CREAT | O_WRONLY | O_APPEND, S_IWUSR | S_IWGRP);

	if (fd < 0) {
		PEPPER_TRACE("Failed to open stdout.txt (errno=%s)\n", strerror(errno));
		return;
	}

	ret = dup2(fd, 1);
	close(fd);
	PEPPER_CHECK(ret >= 0, return, "Failed to redirect STDOUT.\n");

	PEPPER_TRACE("STDOUT has been redirected to stdout.txt.\n");
}

static void
_headless_debug_redir_stderr(headless_debug_t *hdebug, void *data)
{
	(void) hdebug;
	(void) data;

	int fd = -1;
	int ret = 0;

	fd = open("/run/pepper/stderr.txt", O_CREAT | O_WRONLY | O_APPEND, S_IWUSR | S_IWGRP);

	if (fd < 0) {
		PEPPER_TRACE("Failed to open stderr.txt (errno=%s)\n", strerror(errno));
		return;
	}

	ret = dup2(fd, 2);
	close(fd);
	PEPPER_CHECK(ret >= 0, return, "Failed to redirect STDERR.\n");

	PEPPER_TRACE("STDERR has been redirected to stderr.txt.\n");

}

static void
_headless_debug_topvwins(headless_debug_t *hdebug, void *data)
{
	(void) data;

	int cnt = 0;
	int w, h;
	double x, y;
	pid_t pid;

	pepper_list_t *l;
	const pepper_list_t *list;
	pepper_view_t *view;
	pepper_surface_t *surface;
	pepper_view_t *top_visible = NULL;

	PEPPER_CHECK(hdebug, return, "[%s] Invalid headless debug !\n", __FUNCTION__);

	PEPPER_TRACE("No. WinID      RscID       PID     w    h    x    y   Mapped Visible Top Top_Visible Focus\n");
	PEPPER_TRACE("==========================================================================================\n");

	list = pepper_compositor_get_view_list(hdebug->compositor);

	pepper_list_for_each_list(l,  list) {
		view = (pepper_view_t *)l->item;
		PEPPER_CHECK(view, continue, "[%s] Invalid object view:%p\n", __FUNCTION__, view);

		surface = pepper_view_get_surface(view);
		PEPPER_CHECK(surface, continue, "[%s] Invalid object surface:%p\n", __FUNCTION__, surface);

		cnt++;
		pepper_view_get_position(view, &x, &y);
		pepper_view_get_size(view, &w, &h);
		wl_client_get_credentials(wl_resource_get_client(pepper_surface_get_resource(surface)), &pid, NULL, NULL);
		if (!top_visible && pepper_surface_get_buffer(surface))
			top_visible = view;

		pepper_log("DEBUG", PEPPER_LOG_LEVEL_DEBUG, "%3d 0x%08x 0x%08x %5d %4d %4d %4.0f %4.0f     %s       %s     %s       %s       %s\n",
					cnt, surface, pepper_surface_get_resource(surface), pid, w, h, x, y,
					pepper_view_is_mapped(view) ? "O" : "X",
					pepper_view_is_visible(view) ? "O" : "X",
					(hdebug->top_mapped == view) ? "O" : "X",
					(top_visible == view) ? "O" : "X",
					(hdebug->focus == view) ? "O" : "X");
	}

	PEPPER_TRACE("==========================================================================================\n");
}

static void
_headless_debug_keygrab_status(headless_debug_t *hdebug, void *data)
{
	pepper_keyrouter_t *keyrouter;

	keyrouter = headless_input_get_keyrouter(hdebug->compositor);
	pepper_keyrouter_debug_keygrab_status_print(keyrouter);
}

static void
_headless_debug_keymap(headless_debug_t *hdebug, void *data)
{
	pepper_xkb_t *xkb;

	int i;
	int min_keycode, max_keycode, num_mods, num_groups;
	struct xkb_context *context = NULL;
	struct xkb_keymap *keymap = NULL;
	struct xkb_state *state = NULL;
	xkb_keysym_t sym = XKB_KEY_NoSymbol;
	char keyname[256] = {0, };

	xkb = headless_input_get_xkb(hdebug->compositor);
	PEPPER_CHECK(xkb, return, "xkb is not set\n");

	context = pepper_xkb_get_context(xkb);
	PEPPER_CHECK(context, return, "Current pepper_xkb has no context.\n");
	keymap = pepper_xkb_get_keymap(xkb);
	PEPPER_CHECK(keymap, return, "Current pepper_xkb has no keymap.\n");
	state = pepper_xkb_get_state(xkb);
	PEPPER_CHECK(state, return, "Current pepper_xkb has no state.\n");

	min_keycode = xkb_keymap_min_keycode(keymap);
	max_keycode = xkb_keymap_max_keycode(keymap);
	num_groups = xkb_map_num_groups(keymap);
	num_mods = xkb_keymap_num_mods(keymap);

	printf("\n");
	printf("    min keycode: %d\n", min_keycode);
	printf("    max keycode: %d\n", max_keycode);
	printf("    num_groups : %d\n", num_groups);
	printf("    num_mods   : %d\n", num_mods);
	for (i = 0; i < num_mods; i++) {
		printf("        [%2d] mod: %s\n", i, xkb_keymap_mod_get_name(keymap, i));
	}

	printf("\n\n\tkeycode\t\tkeyname\t\t  keysym\t    repeat\n");
	printf("    ----------------------------------------------------------------------\n");

	for (i = min_keycode; i < (max_keycode + 1); i++) {
		sym = xkb_state_key_get_one_sym(state, i);

		memset(keyname, 0, sizeof(keyname));
		xkb_keysym_get_name(sym, keyname, sizeof(keyname));

		if (!strncmp(keyname, "NoSymbol", sizeof("NoSymbol")) && sym == 0x0)
			continue;

		printf("\t%4d%-5s%-25s%-20x%-5d\n", i, "", keyname, sym, xkb_keymap_key_repeats(keymap, i));
	}
}

static const headless_debug_action_t debug_actions[] =
{
	{ STDOUT_REDIR,  _headless_debug_redir_stdout, NULL },
	{ STDERR_REDIR,  _headless_debug_redir_stderr, NULL },
	{ PROTOCOL_TRACE_ON,  _headless_debug_protocol_trace_on, _headless_debug_protocol_trace_off },
	{ PROTOCOL_TRACE_OFF, _headless_debug_protocol_trace_off, NULL },
	{ KEYGRAB_STATUS, _headless_debug_keygrab_status, NULL },
	{ TOPVWINS, _headless_debug_topvwins, NULL },
	{ CONNECTED_CLIENTS, _headless_debug_connected_clients, NULL },
	{ CLIENT_RESOURCES, _headless_debug_connected_clients, NULL },
	{ KEYMAP, _headless_debug_keymap, NULL },
	{ HELP_MSG, _headless_debug_dummy, NULL },
};

static void
_headless_debug_enable_action(headless_debug_t *hdebug, char *cmds)
{
	int n_actions = sizeof(debug_actions)/sizeof(debug_actions[0]);

	for(int n=0 ; n < n_actions ; n++) {
		if (!strncmp(cmds, debug_actions[n].cmds, MAX_CMDS)) {
			PEPPER_TRACE("[%s : %s]\n", __FUNCTION__, debug_actions[n].cmds);
			debug_actions[n].cb(hdebug, (void *)debug_actions[n].cmds);

			break;
		}
	}
}

static void
_headless_debug_disable_action(headless_debug_t *hdebug, char *cmds)
{
	int n_actions = sizeof(debug_actions)/sizeof(debug_actions[0]);

	for(int n=0 ; n < n_actions ; n++) {
		if (!strncmp(cmds, debug_actions[n].cmds, MAX_CMDS)) {
			if (debug_actions[n].disable_cb) {
				PEPPER_TRACE("[%s : %s]\n", __FUNCTION__, debug_actions[n].cmds);
				debug_actions[n].disable_cb(hdebug, (void *)debug_actions[n].cmds);
			}

			break;
		}
	}
}

static void
_trace_cb_handle_inotify_event(uint32_t type, pepper_inotify_event_t *ev, void *data)
{
	headless_debug_t *hdebug = data;
	char *file_name = pepper_inotify_event_name_get(ev);

	PEPPER_CHECK(hdebug, return, "Invalid headless debug instance\n");

	switch (type)
	{
		case PEPPER_INOTIFY_EVENT_TYPE_CREATE:
			_headless_debug_enable_action(hdebug, file_name);
			break;
		case PEPPER_INOTIFY_EVENT_TYPE_REMOVE:
			_headless_debug_disable_action(hdebug, file_name);
			break;
		case PEPPER_INOTIFY_EVENT_TYPE_MODIFY:
			break;
		default:
			PEPPER_TRACE("[%s] Unhandled event type (%d)\n", __FUNCTION__, type);
			break;
	}
}

PEPPER_API void
headless_debug_set_focus_view(pepper_compositor_t *compositor, pepper_view_t *focus_view)
{
	headless_debug_t *hdebug = NULL;

	hdebug = (headless_debug_t *)pepper_object_get_user_data((pepper_object_t *) compositor, &KEY_DEBUG);
	PEPPER_CHECK(hdebug, return, "Invalid headless debug.\n");

	if (hdebug->focus != focus_view) {
		PEPPER_TRACE("[DEBUG] Focus view has been changed to 0x%x (from 0x%x)\n", focus_view, hdebug->focus);
		hdebug->focus = focus_view;
	}
}

PEPPER_API void
headless_debug_set_top_view(pepper_compositor_t *compositor, pepper_view_t *top_view)
{
	headless_debug_t *hdebug = NULL;

	hdebug = (headless_debug_t *)pepper_object_get_user_data((pepper_object_t *) compositor, &KEY_DEBUG);
	PEPPER_CHECK(hdebug, return, "Invalid headless debug.\n");

	if (hdebug->top_mapped != top_view) {
		PEPPER_TRACE("[DEBUG] Top view has been changed to 0x%x (from 0x%x)\n", top_view, hdebug->top_mapped);
		hdebug->top_mapped = top_view;
	}
}

PEPPER_API void
headless_debug_deinit(pepper_compositor_t * compositor)
{
	headless_debug_t *hdebug = NULL;

	hdebug = (headless_debug_t *)pepper_object_get_user_data((pepper_object_t *) compositor, &KEY_DEBUG);
	PEPPER_CHECK(hdebug, return, "Failed to get headless debug instance\n");

	/* remove the directory watching already */
	if (hdebug->inotify)
		pepper_inotify_del(hdebug->inotify, "/run/pepper");

	/* remove inotify */
	pepper_inotify_destroy(hdebug->inotify);
	hdebug->inotify = NULL;

	pepper_object_set_user_data((pepper_object_t *)hdebug->compositor, &KEY_DEBUG, NULL, NULL);
	free(hdebug);
}

pepper_bool_t
headless_debug_init(pepper_compositor_t *compositor)
{
	int n_actions;
	headless_debug_t *hdebug = NULL;
	pepper_inotify_t *inotify = NULL;
	pepper_bool_t res = PEPPER_FALSE;

	hdebug = (headless_debug_t*)calloc(1, sizeof(headless_debug_t));
	PEPPER_CHECK(hdebug, goto error, "Failed to alloc for headless debug\n");
	hdebug->compositor = compositor;

	/* create inotify to watch file(s) for event trace */
	inotify = pepper_inotify_create(hdebug->compositor, _trace_cb_handle_inotify_event, hdebug);
	PEPPER_CHECK(inotify, goto error, "Failed to create inotify\n");

	/* add a directory for watching */
	res = pepper_inotify_add(inotify, "/run/pepper");
	PEPPER_CHECK(res, goto error, "Failed on pepper_inotify_add()\n");

	hdebug->inotify = inotify;
	n_actions = sizeof(debug_actions)/sizeof(debug_actions[0]);

	PEPPER_TRACE("[%s] Done (%d actions have been defined.)\n", __FUNCTION__, n_actions);

	pepper_object_set_user_data((pepper_object_t *)compositor, &KEY_DEBUG, hdebug, NULL);
	return PEPPER_TRUE;

error:
	headless_debug_deinit(compositor);

	return PEPPER_FALSE;
}
