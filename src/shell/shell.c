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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pepper.h>
#include <pepper-output-backend.h>
#include <xdg-shell-unstable-v6-server-protocol.h>
#include <tizen-extension-server-protocol.h>

#include "headless_server.h"

#define UPDATE_SURFACE_TYPE	0		//update the surface_type(map. unmap)
#define SET_UPDATE(x, type)	(x |= ((uint32_t)(1<<type)))
#define IS_UPDATE(x, type)	(!!(x & ((uint32_t)(1<<type))))

typedef enum {
	HEADLESS_SURFACE_NONE,
	HEADLESS_SURFACE_TOPLEVEL,
	HEADLESS_SURFACE_POPUP
} headless_surface_type_t;

typedef struct HEADLESS_SHELL headless_shell_t;
typedef struct HEADLESS_SHELL_SURFACE headless_shell_surface_t;

struct HEADLESS_SHELL{
	pepper_compositor_t *compositor;
	struct wl_global *zxdg_shell;
	struct wl_global *tizen_policy;
	struct wl_event_source *cb_idle;

	pepper_view_t *focus;
	pepper_view_t *top_mapped;
	pepper_view_t *top_visible;

	pepper_event_listener_t *surface_add_listener;
	pepper_event_listener_t *surface_remove_listener;
	pepper_event_listener_t *view_remove_listener;
};

struct HEADLESS_SHELL_SURFACE{
	headless_shell_t *hs_shell;
	pepper_surface_t *surface;
	pepper_view_t *view;
	uint32_t	updates;
	uint8_t		visibility;

	headless_surface_type_t surface_type;
	struct wl_resource *zxdg_shell_surface;
	struct wl_resource *zxdg_surface;	/*resource of toplevel, popup and etc*/
	struct wl_resource *tizen_visibility;
	uint32_t last_ack_configure;

	pepper_bool_t skip_focus;

	pepper_event_listener_t *cb_commit;
};

static void
headless_shell_cb_surface_commit(pepper_event_listener_t *listener,
										pepper_object_t *object,
										uint32_t id, void *info, void *data);
static void
headless_shell_add_idle(headless_shell_t *shell);


static void
headless_shell_send_visiblity(pepper_view_t *view, uint8_t visibility);

const static int KEY_SHELL = 0;

static void
zxdg_toplevel_cb_resource_destroy(struct wl_resource *resource)
{
	headless_shell_surface_t *hs_surface = (headless_shell_surface_t *)wl_resource_get_user_data(resource);

	PEPPER_CHECK(hs_surface, return, "fail to get headless_surface.\n");
	PEPPER_CHECK((hs_surface->surface_type == HEADLESS_SURFACE_TOPLEVEL), return, "Invalid surface type.\n");
	PEPPER_CHECK((hs_surface->zxdg_surface == resource), return, "Invalid surface.");

	PEPPER_TRACE("[SHELL] zxdg_toplevel_cb_resource_destroy: view:%p, hs_surface:%p\n", hs_surface->view, hs_surface);

	if (hs_surface->view) {
		pepper_view_unmap(hs_surface->view);
		headless_shell_add_idle(hs_surface->hs_shell);
	}

	hs_surface->surface_type = HEADLESS_SURFACE_NONE;
	hs_surface->zxdg_surface = NULL;
	SET_UPDATE(hs_surface->updates, UPDATE_SURFACE_TYPE);
}

static void
zxdg_toplevel_cb_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
zxdg_toplevel_cb_parent_set(struct wl_client *client,
			    struct wl_resource *resource,
			    struct wl_resource *res_parent)
{
}

static void
zxdg_toplevel_cb_title_set(struct wl_client *client,
			   struct wl_resource *resource,
			   const char *title)
{
}

static void
zxdg_toplevel_cb_app_id_set(struct wl_client *client,
			    struct wl_resource *resource,
			    const char *app_id)
{
}

static void
zxdg_toplevel_cb_win_menu_show(struct wl_client *client,
			       struct wl_resource *resource,
			       struct wl_resource *res_seat,
			       uint32_t serial,
			       int32_t x,
			       int32_t y)
{
}

static void
zxdg_toplevel_cb_move(struct wl_client *client,
		      struct wl_resource *resource,
		      struct wl_resource *res_seat,
		      uint32_t serial)
{
}

static void
zxdg_toplevel_cb_resize(struct wl_client *client,
			struct wl_resource *resource,
			struct wl_resource *res_seat,
			uint32_t serial,
			uint32_t edges)
{
}

static void
zxdg_toplevel_cb_max_size_set(struct wl_client *client,
			      struct wl_resource *resource,
			      int32_t w,
			      int32_t h)
{
}

static void
zxdg_toplevel_cb_min_size_set(struct wl_client *client,
			      struct wl_resource *resource,
			      int32_t w,
			      int32_t h)
{
}

static void
zxdg_toplevel_cb_maximized_set(struct wl_client *client, struct wl_resource *resource)
{
}

static void
zxdg_toplevel_cb_maximized_unset(struct wl_client *client, struct wl_resource *resource)
{
}

static void
zxdg_toplevel_cb_fullscreen_set(struct wl_client *client,
				struct wl_resource *resource,
				struct wl_resource *res_output)
{
}

static void
zxdg_toplevel_cb_fullscreen_unset(struct wl_client *client, struct wl_resource *resource)
{
}

static void
zxdg_toplevel_cb_minimized_set(struct wl_client *client, struct wl_resource *resource)
{
}

static const struct zxdg_toplevel_v6_interface zxdg_toplevel_interface =
{
	zxdg_toplevel_cb_destroy,
	zxdg_toplevel_cb_parent_set,
	zxdg_toplevel_cb_title_set,
	zxdg_toplevel_cb_app_id_set,
	zxdg_toplevel_cb_win_menu_show,
	zxdg_toplevel_cb_move,
	zxdg_toplevel_cb_resize,
	zxdg_toplevel_cb_max_size_set,
	zxdg_toplevel_cb_min_size_set,
	zxdg_toplevel_cb_maximized_set,
	zxdg_toplevel_cb_maximized_unset,
	zxdg_toplevel_cb_fullscreen_set,
	zxdg_toplevel_cb_fullscreen_unset,
	zxdg_toplevel_cb_minimized_set
};

static void
zxdg_popup_cb_resource_destroy(struct wl_resource *resource)
{
	headless_shell_surface_t *hs_surface = (headless_shell_surface_t *)wl_resource_get_user_data(resource);

	PEPPER_CHECK(hs_surface, return, "fail to get headless_surface.\n");
	PEPPER_CHECK((hs_surface->surface_type == HEADLESS_SURFACE_POPUP), return, "Invalid surface type.\n");
	PEPPER_CHECK((hs_surface->zxdg_surface == resource), return, "Invalid surface.");

	hs_surface->surface_type = HEADLESS_SURFACE_NONE;
	hs_surface->zxdg_surface = NULL;
	SET_UPDATE(hs_surface->updates, UPDATE_SURFACE_TYPE);
}

static void
zxdg_popup_cb_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
zxdg_popup_cb_grab(struct wl_client *client,
		   struct wl_resource *resource,
		   struct wl_resource *res_seat,
		   uint32_t serial)
{
}

static const struct zxdg_popup_v6_interface zxdg_popup_interface =
{
	zxdg_popup_cb_destroy,
	zxdg_popup_cb_grab
};

static void
zxdg_surface_cb_resource_destroy(struct wl_resource *resource)
{
	headless_shell_surface_t *hs_surface;

	hs_surface = wl_resource_get_user_data(resource);
	PEPPER_CHECK(hs_surface, return, "fail to get hs_surface\n");

	PEPPER_TRACE("[SHELL] zxdg_surface_cb_resource_destroy: hs_surface:%p view:%p\n", hs_surface, hs_surface->view);

	if (hs_surface->view) {
		pepper_view_destroy(hs_surface->view);
		hs_surface->view = NULL;
	}

	if (hs_surface->cb_commit) {
		pepper_event_listener_remove(hs_surface->cb_commit);
		hs_surface->cb_commit = NULL;
	}

	hs_surface->zxdg_shell_surface = NULL;
	hs_surface->skip_focus = PEPPER_FALSE;
	hs_surface->visibility = TIZEN_VISIBILITY_VISIBILITY_FULLY_OBSCURED;

	SET_UPDATE(hs_surface->updates, UPDATE_SURFACE_TYPE);
	headless_shell_add_idle(hs_surface->hs_shell);
}

static void
zxdg_surface_cb_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
zxdg_surface_cb_toplevel_get(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
	headless_shell_surface_t *hs_surface = (headless_shell_surface_t *)wl_resource_get_user_data(resource);
	struct wl_resource *new_res;

	PEPPER_CHECK(hs_surface, return, "fail to get headless_surface\n");
	PEPPER_CHECK((hs_surface->zxdg_surface == NULL), return, "alwreay assign zdg_surface:%p role:%d\n", hs_surface->zxdg_surface, hs_surface->surface_type);

	new_res = wl_resource_create(client, &zxdg_toplevel_v6_interface, 1, id);
	if (!new_res) {
		PEPPER_ERROR("fail to create zxdg_toplevel");
		wl_resource_post_no_memory(resource);
		return;
	}

	wl_resource_set_implementation(new_res,
			&zxdg_toplevel_interface,
			hs_surface,
			zxdg_toplevel_cb_resource_destroy);

	hs_surface->surface_type = HEADLESS_SURFACE_TOPLEVEL;
	hs_surface->zxdg_surface = new_res;

	SET_UPDATE(hs_surface->updates, UPDATE_SURFACE_TYPE);
}

static void
zxdg_surface_cb_popup_get(struct wl_client *client,
									struct wl_resource *resource,
									uint32_t id,
									struct wl_resource *res_parent,
									struct wl_resource *res_pos)
{
	headless_shell_surface_t *hs_surface = (headless_shell_surface_t *)wl_resource_get_user_data(resource);
	struct wl_resource *new_res;

	PEPPER_CHECK(hs_surface, return, "fail to get headless_surface\n");
	PEPPER_CHECK((hs_surface->zxdg_surface == NULL), return, "alwreay assign zdg_surface:%p role:%d\n", hs_surface->zxdg_surface, hs_surface->surface_type);

	new_res = wl_resource_create(client, &zxdg_popup_v6_interface, 1, id);
	if (!new_res) {
		PEPPER_ERROR("fail to create popup");
		wl_resource_post_no_memory(resource);
		return;
	}

	wl_resource_set_implementation(new_res,
	                          &zxdg_popup_interface,
	                          hs_surface,
	                          zxdg_popup_cb_resource_destroy);

	hs_surface->surface_type = HEADLESS_SURFACE_POPUP;
	hs_surface->zxdg_surface = new_res;

	SET_UPDATE(hs_surface->updates, UPDATE_SURFACE_TYPE);
}

static void
zxdg_surface_cb_win_geometry_set(struct wl_client *client,
                                   struct wl_resource *resource,
                                   int32_t x,
                                   int32_t y,
                                   int32_t w,
                                   int32_t h)
{
}

static void
zxdg_surface_cb_configure_ack(struct wl_client *client, struct wl_resource *resource, uint32_t serial)
{
	headless_shell_surface_t *hs_surface;

	hs_surface = wl_resource_get_user_data(resource);
	PEPPER_CHECK(hs_surface, return, "fail to get headless_shell_surface\n");

	hs_surface->last_ack_configure = serial;
}

static const struct zxdg_surface_v6_interface zxdg_surface_interface =
{
	zxdg_surface_cb_destroy,
	zxdg_surface_cb_toplevel_get,
	zxdg_surface_cb_popup_get,
	zxdg_surface_cb_win_geometry_set,
	zxdg_surface_cb_configure_ack
};

static void
zxdg_positioner_cb_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
zxdg_positioner_cb_size_set(struct wl_client *client,
			    struct wl_resource *resource,
			    int32_t w, int32_t h)
{
}

static void
zxdg_positioner_cb_anchor_rect_set(struct wl_client *client,
				   struct wl_resource *resource,
				   int32_t x, int32_t y, int32_t w, int32_t h)
{
}

static void
zxdg_positioner_cb_anchor_set(struct wl_client *client,
			      struct wl_resource *resource,
			      enum zxdg_positioner_v6_anchor anchor)
{
}

static void
zxdg_positioner_cb_gravity_set(struct wl_client *client,
			       struct wl_resource *resource,
			       enum zxdg_positioner_v6_gravity gravity)
{
}

static void
zxdg_positioner_cb_constraint_adjustment_set(struct wl_client *client,
					     struct wl_resource *resource,
					     enum zxdg_positioner_v6_constraint_adjustment constraint_adjustment)
{
}

static void
zxdg_positioner_cb_offset_set(struct wl_client *client,
			      struct wl_resource *resource,
			      int32_t x, int32_t y)
{
}

static const struct zxdg_positioner_v6_interface zxdg_positioner_interface =
{
	zxdg_positioner_cb_destroy,
	zxdg_positioner_cb_size_set,
	zxdg_positioner_cb_anchor_rect_set,
	zxdg_positioner_cb_anchor_set,
	zxdg_positioner_cb_gravity_set,
	zxdg_positioner_cb_constraint_adjustment_set,
	zxdg_positioner_cb_offset_set,
};

static void
zxdg_shell_cb_destroy(struct wl_client *client, struct wl_resource *resource)
{
	PEPPER_TRACE("Destroy zxdg_shell\n");

	wl_resource_destroy(resource);
}

static void
zxdg_shell_cb_positioner_create(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
	struct wl_resource *new_res;

	PEPPER_TRACE("Create zxdg_positoiner\n");

	new_res = wl_resource_create(client, &zxdg_positioner_v6_interface, 1, id);
	if (!new_res)
	{
		PEPPER_ERROR("fail to create zxdg_positioner\n");
		wl_resource_post_no_memory(resource);
		return;
	}

	wl_resource_set_implementation(new_res, &zxdg_positioner_interface, NULL, NULL);
}

static void
zxdg_shell_cb_surface_get(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *wsurface)
{
	headless_shell_t *hs;
	headless_shell_surface_t *hs_surface = NULL;
	pepper_surface_t *psurface;
	const char *role;

	hs = wl_resource_get_user_data(resource);
	if (!hs) {
		PEPPER_ERROR("fail to get headless_shell\n");
		wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
				"failed to get headless shell");
		return;
	}

	psurface = wl_resource_get_user_data(wsurface);
	PEPPER_CHECK(psurface, goto error, "faile to get pepper_surface\n");

	hs_surface = (headless_shell_surface_t *)pepper_object_get_user_data((pepper_object_t *)psurface, wsurface);
	if (!hs_surface) {
		PEPPER_ERROR("fail to get headless_shell_surface_t: %p key:%p\n", psurface, wsurface);
		wl_resource_post_no_memory(resource);
		return;
	}

	hs_surface->zxdg_shell_surface = wl_resource_create(client, &zxdg_surface_v6_interface, 1, id);
	if (!hs_surface->zxdg_shell_surface) {
		PEPPER_ERROR("fail to create the zxdg_surface\n");
		wl_resource_post_no_memory(resource);
		goto error;
	}

	wl_resource_set_implementation(hs_surface->zxdg_shell_surface,
			&zxdg_surface_interface,
			hs_surface,
			zxdg_surface_cb_resource_destroy);

	hs_surface->view = pepper_compositor_add_view(hs->compositor);
	if (!hs_surface->view) {
		PEPPER_ERROR("fail to create the pepper_view\n");
		wl_resource_post_no_memory(resource);
		goto error;
	}

	if (!pepper_view_set_surface(hs_surface->view, psurface)) {
		PEPPER_ERROR("fail to set surface\n");
		wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
				"Assign set psurface to pview");
		goto error;
	}

	hs_surface->cb_commit = pepper_object_add_event_listener((pepper_object_t *)psurface,
															PEPPER_EVENT_SURFACE_COMMIT, 0, headless_shell_cb_surface_commit, hs_surface);

	role = pepper_surface_get_role(psurface);
	if (!role) {
		if (!pepper_surface_set_role(psurface, "xdg_surface")) {
			PEPPER_ERROR("fail to set role\n");
			wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
					"Assign \"xdg_surface\" to wl_surface failed\n");
			goto error;
		}
	} else {
		PEPPER_CHECK(!strcmp(role, "xdg_surface"), goto error, "surface has alweady role %s\n", role);
	}

	PEPPER_TRACE("[SHELL] create zxdg_surface:%p, pview:%p, psurface:%p\n",
			hs_surface->zxdg_shell_surface,
			hs_surface->view,
			psurface);

	return;
error:
	if (hs_surface) {
		if (hs_surface->view) {
			pepper_view_destroy(hs_surface->view);
			hs_surface->view = NULL;
		}

		if (hs_surface->zxdg_shell_surface) {
			wl_resource_destroy(hs_surface->zxdg_shell_surface);
			hs_surface->zxdg_shell_surface = NULL;
		}
	}
}

static void
zxdg_shell_cb_pong(struct wl_client *client, struct wl_resource *resource, uint32_t serial)
{

}

static const struct zxdg_shell_v6_interface zxdg_shell_interface =
{
	zxdg_shell_cb_destroy,
	zxdg_shell_cb_positioner_create,
	zxdg_shell_cb_surface_get,
	zxdg_shell_cb_pong
};

static void
zxdg_shell_cb_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_resource *resource;
	headless_shell_t *hs = (headless_shell_t *)data;

	PEPPER_TRACE("Bind zxdg_shell\n");

	/* Create resource for zxdg_shell_v6 */
	resource = wl_resource_create(client,
			&zxdg_shell_v6_interface,
			version,
			id);
	PEPPER_CHECK(resource, goto err_shell, "fail to create the zxdg_shell_v6\n");

	wl_resource_set_implementation(resource, &zxdg_shell_interface, hs, NULL);

	return;
err_shell:
	wl_resource_destroy(resource);
	wl_client_post_no_memory(client);
}

static pepper_bool_t
zxdg_init(headless_shell_t *shell)
{
	struct wl_display *display;

	display = pepper_compositor_get_display(shell->compositor);

	shell->zxdg_shell = wl_global_create(display, &zxdg_shell_v6_interface, 1, shell, zxdg_shell_cb_bind);
	PEPPER_CHECK(shell->zxdg_shell, return PEPPER_FALSE, "fail to create zxdg_shell\n");

	return PEPPER_TRUE;
}

void
zxdg_deinit(headless_shell_t *shell)
{
	if (shell->zxdg_shell)
		wl_global_destroy(shell->zxdg_shell);
}

static void
tizen_visibility_cb_destroy(struct wl_client *client, struct wl_resource *res_tzvis)
{
	wl_resource_destroy(res_tzvis);
}

static const struct tizen_visibility_interface tizen_visibility =
{
	tizen_visibility_cb_destroy
};

static void
tizen_visibility_cb_vis_destroy(struct wl_resource *res_tzvis)
{
	headless_shell_surface_t *hs_surface = (headless_shell_surface_t *)wl_resource_get_user_data(res_tzvis);

	PEPPER_CHECK(hs_surface, return, "[SHELL] cannot get headless_shell_surface_t\n");
	hs_surface->tizen_visibility = NULL;
}

static void
tizen_position_cb_destroy(struct wl_client *client, struct wl_resource *res_tzpos)
{
   wl_resource_destroy(res_tzpos);
}

static void
tizen_position_cb_set(struct wl_client *client, struct wl_resource *res_tzpos, int32_t x, int32_t y)
{
}

static const struct tizen_position_interface tizen_position =
{
   tizen_position_cb_destroy,
   tizen_position_cb_set,
};

static void
tizen_position_cb_pos_destroy(struct wl_resource *res_tzpos)
{
}

static void
tizen_policy_cb_vis_get(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surf)
{
	pepper_surface_t *psurface;
	headless_shell_surface_t *hs_surface;
	struct wl_resource *new_res;

	psurface = wl_resource_get_user_data(surf);
	PEPPER_CHECK(psurface, return, "fail to get pepper_surface_t\n");

	hs_surface = pepper_object_get_user_data((pepper_object_t *)psurface, surf);
	PEPPER_CHECK(hs_surface, return, "fail to get headless_shell_surface\n");

	new_res = wl_resource_create(client, &tizen_visibility_interface, 5, id);
	if (!new_res) {
		PEPPER_ERROR("fail to create tizen_visibility");
		wl_resource_post_no_memory(resource);
		return;
	}

	wl_resource_set_implementation(new_res,
			&tizen_visibility,
			hs_surface,
			tizen_visibility_cb_vis_destroy);

	hs_surface->tizen_visibility = new_res;

	if (hs_surface->visibility != TIZEN_VISIBILITY_VISIBILITY_FULLY_OBSCURED)
		tizen_visibility_send_notify(hs_surface->tizen_visibility, hs_surface->visibility);
}

static void
tizen_policy_cb_pos_get(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surf)
{
	struct wl_resource *new_res;

	new_res = wl_resource_create(client, &tizen_position_interface, 1, id);
	if (!new_res) {
		PEPPER_ERROR("fail to create tizen_visibility");
		wl_resource_post_no_memory(resource);
		return;
	}

	wl_resource_set_implementation(new_res,
			&tizen_position,
			NULL,
			tizen_position_cb_pos_destroy);
}

static void
tizen_policy_cb_activate(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surf)
{
	pepper_surface_t *psurface;
	headless_shell_surface_t *hs_surface;

	psurface = wl_resource_get_user_data(surf);
	PEPPER_CHECK(psurface, return, "fail to get pepper_surface_t\n");

	hs_surface = pepper_object_get_user_data((pepper_object_t *)psurface, surf);
	PEPPER_CHECK(hs_surface, return, "fail to get headless_shell_surface\n");
	PEPPER_CHECK(hs_surface->view, return, "invalid view from headless_shell_surface\n");

	pepper_view_stack_top(hs_surface->view, PEPPER_TRUE);

	headless_shell_add_idle(hs_surface->hs_shell);
}

static void
tizen_policy_cb_activate_below_by_res_id(struct wl_client *client, struct wl_resource *resource,  uint32_t res_id, uint32_t below_res_id)
{
}

static void
tizen_policy_cb_raise(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surf)
{
}

static void
tizen_policy_cb_lower(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surf)
{
}

static void
tizen_policy_cb_lower_by_res_id(struct wl_client *client, struct wl_resource *resource,  uint32_t res_id)
{
}

static void
tizen_policy_cb_focus_skip_set(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surf)
{
	pepper_surface_t *psurface;
	headless_shell_surface_t *hs_surface;

	psurface = wl_resource_get_user_data(surf);
	PEPPER_CHECK(psurface, return, "fail to get pepper_surface_t\n");

	hs_surface = pepper_object_get_user_data((pepper_object_t *)psurface, surf);
	PEPPER_CHECK(hs_surface, return, "fail to get headless_shell_surface\n");

	hs_surface->skip_focus = PEPPER_TRUE;
}

static void
tizen_policy_cb_focus_skip_unset(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surf)
{
	pepper_surface_t *psurface;
	headless_shell_surface_t *hs_surface;

	psurface = wl_resource_get_user_data(surf);
	PEPPER_CHECK(psurface, return, "fail to get pepper_surface_t\n");

	hs_surface = pepper_object_get_user_data((pepper_object_t *)psurface, surf);
	PEPPER_CHECK(hs_surface, return, "fail to get headless_shell_surface\n");

	hs_surface->skip_focus = PEPPER_FALSE;
}

static void
tizen_policy_cb_role_set(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surf, const char *role)
{
}

static void
tizen_policy_cb_type_set(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surf, uint32_t type)
{
}

static void
tizen_policy_cb_conformant_set(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surf)
{
}

static void
tizen_policy_cb_conformant_unset(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surf)
{
}

static void
tizen_policy_cb_conformant_get(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surf)
{
   tizen_policy_send_conformant(resource, surf, 0);
}

static void
tizen_policy_cb_notilv_set(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surf, int32_t lv)
{
}

static void
tizen_policy_cb_transient_for_set(struct wl_client *client, struct wl_resource *resource, uint32_t child_id, uint32_t parent_id)
{
}

static void
tizen_policy_cb_transient_for_unset(struct wl_client *client, struct wl_resource *resource, uint32_t child_id)
{
   tizen_policy_send_transient_for_done(resource, child_id);
}

static void
tizen_policy_cb_win_scrmode_set(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surf, uint32_t mode)
{
   tizen_policy_send_window_screen_mode_done
     (resource, surf, mode, TIZEN_POLICY_ERROR_STATE_NONE);
}

static void
tizen_policy_cb_subsurf_place_below_parent(struct wl_client *client, struct wl_resource *resource, struct wl_resource *subsurf)
{
}

static void
tizen_policy_cb_subsurf_stand_alone_set(struct wl_client *client, struct wl_resource *resource, struct wl_resource *subsurf)
{
}

static void
tizen_policy_cb_subsurface_get(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface, uint32_t parent_id)
{
	wl_client_post_implementation_error(client, "Headless server not support tizen_subsurface\n");
}

static void
tizen_policy_cb_opaque_state_set(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surface, int32_t state)
{
}

static void
tizen_policy_cb_iconify(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surf)
{
}

static void
tizen_policy_cb_uniconify(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surf)
{
}

static void
tizen_policy_cb_aux_hint_add(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surf, int32_t id, const char *name, const char *value)
{
}

static void
tizen_policy_cb_aux_hint_change(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surf, int32_t id, const char *value)
{
}

static void
tizen_policy_cb_aux_hint_del(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surf, int32_t id)
{
}

static void
tizen_policy_cb_supported_aux_hints_get(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surf)
{
}

static void
tizen_policy_cb_background_state_set(struct wl_client *client, struct wl_resource *resource, uint32_t pid)
{
}

static void
tizen_policy_cb_background_state_unset(struct wl_client *client, struct wl_resource *resource, uint32_t pid)
{
}

static void
tizen_policy_cb_floating_mode_set(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surf)
{
}

static void
tizen_policy_cb_floating_mode_unset(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surf)
{
}

static void
tizen_policy_cb_stack_mode_set(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surf, uint32_t mode)
{
}

static void
tizen_policy_cb_activate_above_by_res_id(struct wl_client *client, struct wl_resource *resource,  uint32_t res_id, uint32_t above_res_id)
{
}

static void
tizen_policy_cb_subsurf_watcher_get(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface)
{
	wl_client_post_implementation_error(client, "Headless server not support tizen_subsurface\n");
}

static void
tizen_policy_cb_parent_set(struct wl_client *client, struct wl_resource *resource, struct wl_resource *child, struct wl_resource *parent)
{
}

static void
tizen_policy_cb_ack_conformant_region(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surface, uint32_t serial)
{
}

static void
tizen_policy_cb_destroy(struct wl_client *client, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void
tizen_policy_cb_has_video(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surface, uint32_t has)
{
}

static const struct tizen_policy_interface tizen_policy_iface =
{
   tizen_policy_cb_vis_get,
   tizen_policy_cb_pos_get,
   tizen_policy_cb_activate,
   tizen_policy_cb_activate_below_by_res_id,
   tizen_policy_cb_raise,
   tizen_policy_cb_lower,
   tizen_policy_cb_lower_by_res_id,
   tizen_policy_cb_focus_skip_set,
   tizen_policy_cb_focus_skip_unset,
   tizen_policy_cb_role_set,
   tizen_policy_cb_type_set,
   tizen_policy_cb_conformant_set,
   tizen_policy_cb_conformant_unset,
   tizen_policy_cb_conformant_get,
   tizen_policy_cb_notilv_set,
   tizen_policy_cb_transient_for_set,
   tizen_policy_cb_transient_for_unset,
   tizen_policy_cb_win_scrmode_set,
   tizen_policy_cb_subsurf_place_below_parent,
   tizen_policy_cb_subsurf_stand_alone_set,
   tizen_policy_cb_subsurface_get,
   tizen_policy_cb_opaque_state_set,
   tizen_policy_cb_iconify,
   tizen_policy_cb_uniconify,
   tizen_policy_cb_aux_hint_add,
   tizen_policy_cb_aux_hint_change,
   tizen_policy_cb_aux_hint_del,
   tizen_policy_cb_supported_aux_hints_get,
   tizen_policy_cb_background_state_set,
   tizen_policy_cb_background_state_unset,
   tizen_policy_cb_floating_mode_set,
   tizen_policy_cb_floating_mode_unset,
   tizen_policy_cb_stack_mode_set,
   tizen_policy_cb_activate_above_by_res_id,
   tizen_policy_cb_subsurf_watcher_get,
   tizen_policy_cb_parent_set,
   tizen_policy_cb_ack_conformant_region,
   tizen_policy_cb_destroy,
   tizen_policy_cb_has_video,
};

static void
tizen_policy_cb_unbind(struct wl_resource *resource)
{
	headless_shell_t *shell = (headless_shell_t *)wl_resource_get_user_data(resource);

	shell->tizen_policy = NULL;
}

static void
tizen_policy_cb_bind(struct wl_client *client, void *data, uint32_t ver, uint32_t id)
{
	headless_shell_t *shell = (headless_shell_t *)data;
	struct wl_resource *resource;

	PEPPER_CHECK(shell, goto err, "data is NULL\n");

	resource = wl_resource_create(client,
									&tizen_policy_interface,
									ver,
									id);
	PEPPER_CHECK(resource, goto err, "fail to create tizen_policy\n");

	wl_resource_set_implementation(resource,
									&tizen_policy_iface,
									shell,
									tizen_policy_cb_unbind);
	return;

err:
	wl_client_post_no_memory(client);
}

static pepper_bool_t
tizen_policy_init(headless_shell_t *shell)
{
	struct wl_display *display;

	display = pepper_compositor_get_display(shell->compositor);

	shell->tizen_policy = wl_global_create(display, &tizen_policy_interface, 7, shell, tizen_policy_cb_bind);
	PEPPER_CHECK(shell->tizen_policy, return PEPPER_FALSE, "faile to create tizen_policy\n");

	return PEPPER_TRUE;
}

void
tizen_policy_deinit(headless_shell_t *shell)
{
	if (shell->tizen_policy)
		wl_global_destroy(shell->tizen_policy);
}

static void
headless_shell_send_visiblity(pepper_view_t *view, uint8_t visibility)
{
	pepper_surface_t *surface;
	headless_shell_surface_t *hs_surface;

	if (view == NULL) return;

	surface = pepper_view_get_surface(view);
	PEPPER_CHECK(surface, return, "[SHELL] Invalid object surface:%p\n", surface);

	hs_surface = pepper_object_get_user_data((pepper_object_t *)surface, pepper_surface_get_resource(surface));
	PEPPER_CHECK(hs_surface, return, "[SHELL] Invalid object headless_surface:%p\n", hs_surface);

	if (hs_surface->visibility == visibility) {
		PEPPER_TRACE("[SHELL] Same Visibility hs_surface:%p, visibility:%d\n", hs_surface, visibility);
		return;
	}

	if (hs_surface->tizen_visibility)
		tizen_visibility_send_notify(hs_surface->tizen_visibility, visibility);

	hs_surface->visibility = visibility;
	PEPPER_TRACE("[SHELL] Set Visibility hs_surface:%p, visibility:%d\n", hs_surface, visibility);
}

static void
headless_shell_cb_idle(void *data)
{
	headless_shell_t *hs_shell = (headless_shell_t *)data;
	const pepper_list_t *list;
	pepper_list_t *l;
	pepper_view_t *view;
	pepper_surface_t *surface;
	headless_shell_surface_t *hs_surface;

	pepper_view_t *focus = NULL, *top = NULL, *top_visible = NULL;

	PEPPER_TRACE("[SHELL] Enter Idle\n");
	list = pepper_compositor_get_view_list(hs_shell->compositor);

	pepper_list_for_each_list(l,  list) {
		view = (pepper_view_t *)l->item;
		PEPPER_CHECK(view, continue, "[SHELL] idle_cb, Invalid object view:%p\n", view);

		surface = pepper_view_get_surface(view);
		PEPPER_CHECK(surface, continue, "[SHELL] idle_cb, Invalid object surface:%p\n", surface);

		hs_surface = pepper_object_get_user_data((pepper_object_t *)surface, pepper_surface_get_resource(surface));
		PEPPER_CHECK(hs_surface, continue, "[SHELL] idle_cb, Invalid object headless_surface:%p\n", hs_surface);

		if (!pepper_view_is_mapped(view)) {
			headless_shell_send_visiblity(view, TIZEN_VISIBILITY_VISIBILITY_FULLY_OBSCURED);
			continue;
		}

		if (!top)
			top = view;

		if (!focus && !hs_surface->skip_focus)
			focus = view;

		if (!top_visible && pepper_surface_get_buffer(surface))
			top_visible = view;

		if (top && focus && top_visible)
			break;
	}

	if (top != hs_shell->top_mapped) {
		const pepper_list_t *l;
		pepper_list_t *ll;

		PEPPER_TRACE("[SHELL] IDLE : top-view change: %p to %p\n", hs_shell->top_mapped , top);
		hs_shell->top_mapped = top;
		headless_input_set_top_view(hs_shell->compositor, hs_shell->top_mapped);
		headless_debug_set_top_view(hs_shell->compositor, hs_shell->top_mapped);

		/*Force update the output*/
		l = pepper_compositor_get_output_list(hs_shell->compositor);
		pepper_list_for_each_list(ll, l) {
			pepper_output_add_damage_region((pepper_output_t *)ll->item, NULL);
		}
	}

	if (focus != hs_shell->focus) {
		PEPPER_TRACE("[SHELL] IDLE : focus-view change: %p to %p\n", hs_shell->focus , focus);
		hs_shell->focus = focus;
		headless_input_set_focus_view(hs_shell->compositor, hs_shell->focus);
		headless_debug_set_focus_view(hs_shell->compositor, hs_shell->focus);
	}

	if (top_visible != hs_shell->top_visible) {
		PEPPER_TRACE("[SHELL] IDLE : visible-view change: %p to %p\n", hs_shell->top_visible, top_visible);
		headless_shell_send_visiblity(hs_shell->top_visible, TIZEN_VISIBILITY_VISIBILITY_FULLY_OBSCURED);
		headless_shell_send_visiblity(top_visible, TIZEN_VISIBILITY_VISIBILITY_UNOBSCURED);
		hs_shell->top_visible = top_visible;
	}

	hs_shell->cb_idle = NULL;
}

static void
headless_shell_cb_surface_commit(pepper_event_listener_t *listener,
										pepper_object_t *object,
										uint32_t id, void *info, void *data)
{
	headless_shell_surface_t * hs_surface = (headless_shell_surface_t *)data;

	PEPPER_CHECK(((pepper_object_t *)hs_surface->surface == object), return, "Invalid object\n");

	/*TODO
	1. Check the changes(buffer, map status...)
	*/
	if (IS_UPDATE(hs_surface->updates, UPDATE_SURFACE_TYPE)) {
		if (hs_surface->surface_type != HEADLESS_SURFACE_NONE)
			pepper_view_map(hs_surface->view);
		else
			pepper_view_unmap(hs_surface->view);

		PEPPER_TRACE("Surface type change. view:%p, type:%d, res:%p\n", hs_surface->view, hs_surface->surface_type, hs_surface->zxdg_surface);
	}

	hs_surface->updates = 0;

	headless_shell_add_idle(hs_surface->hs_shell);
}

static void
headless_shell_cb_surface_free(void *data)
{
	headless_shell_surface_t *surface = (headless_shell_surface_t *)data;

	PEPPER_TRACE("[SHELL] hs_surface free surface:%p, view:%p, zxdg_shell_surface:%p, zxdg_surface:%p\n",
					surface->surface, surface->view,
					surface->zxdg_shell_surface, surface->zxdg_surface);

	free(surface);
}

static void
headless_shell_cb_surface_add(pepper_event_listener_t *listener,
										pepper_object_t *object,
										uint32_t id, void *info, void *data)
{
	headless_shell_surface_t *hs_surface;
	pepper_surface_t *surface = (pepper_surface_t *)info;

	hs_surface = (headless_shell_surface_t*)calloc(sizeof(headless_shell_surface_t), 1);
	PEPPER_CHECK(hs_surface, return, "fail to alloc for headless_shell_surface\n");

	hs_surface->hs_shell = (headless_shell_t *)data;
	hs_surface->surface = (pepper_surface_t *)surface;
	hs_surface->visibility = TIZEN_VISIBILITY_VISIBILITY_FULLY_OBSCURED;

	pepper_object_set_user_data((pepper_object_t *)surface,
								pepper_surface_get_resource(surface),
								hs_surface,
								headless_shell_cb_surface_free);
	PEPPER_TRACE("[SHELL] surface_Add: pepper_surface:%, headless_shell:%p to p\n", surface, hs_surface);
}

static void
headless_shell_cb_surface_remove(pepper_event_listener_t *listener,
										pepper_object_t *object,
										uint32_t id, void *info, void *data)
{
	headless_shell_surface_t *hs_surface;
	pepper_surface_t *surface = (pepper_surface_t *)info;

	hs_surface = pepper_object_get_user_data((pepper_object_t *)surface, pepper_surface_get_resource(surface));
	PEPPER_TRACE("[SHELL] surface_remove: pepper_surface:%p, headless_shell:%p\n", object, hs_surface);

	if (hs_surface->zxdg_surface) {
		wl_resource_set_user_data(hs_surface->zxdg_surface, NULL);
		hs_surface->zxdg_surface = NULL;
	}

	if (hs_surface->zxdg_shell_surface) {
		wl_resource_set_user_data(hs_surface->zxdg_shell_surface, NULL);
		hs_surface->zxdg_shell_surface = NULL;
	}

	if (hs_surface->view) {
		pepper_view_destroy(hs_surface->view);
		hs_surface->view = NULL;
	}

	SET_UPDATE(hs_surface->updates, UPDATE_SURFACE_TYPE);
	headless_shell_add_idle(hs_surface->hs_shell);
}

static void
headless_shell_cb_view_remove(pepper_event_listener_t *listener,
										pepper_object_t *object,
										uint32_t id, void *info, void *data)
{
	pepper_view_t *view = (pepper_view_t *)info;
	headless_shell_t *shell = (headless_shell_t *)data;

	if (view == shell->top_mapped)
		shell->top_mapped = NULL;

	if (view == shell->top_visible)
		shell->top_visible = NULL;

	if (view == shell->focus)
		shell->focus = NULL;

	headless_shell_add_idle(shell);
}

static void
headless_shell_add_idle(headless_shell_t *shell)
{
	struct wl_event_loop *loop;

	if (!shell || shell->cb_idle)
		return;

	loop = wl_display_get_event_loop(pepper_compositor_get_display(shell->compositor));
	PEPPER_CHECK(loop, return, "fail to get event loop\n");

	shell->cb_idle = wl_event_loop_add_idle(loop, headless_shell_cb_idle, shell);
	PEPPER_CHECK(shell->cb_idle, return, "fail to add idle\n");
}

static void
headless_shell_init_listeners(headless_shell_t *shell)
{
	shell->surface_add_listener = pepper_object_add_event_listener((pepper_object_t *)shell->compositor,
																	PEPPER_EVENT_COMPOSITOR_SURFACE_ADD,
																	0, headless_shell_cb_surface_add, shell);

	shell->surface_remove_listener = pepper_object_add_event_listener((pepper_object_t *)shell->compositor,
																		PEPPER_EVENT_COMPOSITOR_SURFACE_REMOVE,
																		0, headless_shell_cb_surface_remove, shell);

	shell->view_remove_listener = pepper_object_add_event_listener((pepper_object_t *)shell->compositor,
																		PEPPER_EVENT_COMPOSITOR_VIEW_REMOVE,
																		0, headless_shell_cb_view_remove, shell);
}

static void
headless_shell_deinit_listeners(headless_shell_t *shell)
{
	pepper_event_listener_remove(shell->surface_add_listener);
	pepper_event_listener_remove(shell->surface_remove_listener);
	pepper_event_listener_remove(shell->view_remove_listener);
}

static void
headless_shell_destroy(headless_shell_t *shell)
{
	if (!shell)
		return;

	if (shell->cb_idle)
		wl_event_source_remove(shell->cb_idle);

	headless_shell_deinit_listeners(shell);
	zxdg_deinit(shell);
	tizen_policy_deinit(shell);
}

void
headless_shell_deinit(pepper_compositor_t *compositor)
{
	headless_shell_t *shell;

	PEPPER_CHECK(compositor, return, "compositor is NULL\n");

	shell = (headless_shell_t *)pepper_object_get_user_data((pepper_object_t *)compositor, &KEY_SHELL);
	PEPPER_CHECK(shell, return, "shell is NULL\n");

	headless_shell_destroy(shell);

	pepper_object_set_user_data((pepper_object_t *)shell->compositor, &KEY_SHELL, NULL, NULL);
	free(shell);
}

pepper_bool_t
headless_shell_init(pepper_compositor_t *compositor)
{
	headless_shell_t *shell;

	shell = (headless_shell_t*)calloc(sizeof(headless_shell_t), 1);
	PEPPER_CHECK(shell, goto error, "fail to alloc for shell\n");
	shell->compositor = compositor;

	headless_shell_init_listeners(shell);
	PEPPER_CHECK(zxdg_init(shell), goto error, "zxdg_init() failed\n");
	PEPPER_CHECK(tizen_policy_init(shell), goto error, "tizen_policy_init() failed\n");

	pepper_object_set_user_data((pepper_object_t *)compositor, &KEY_SHELL, shell, NULL);

	return PEPPER_TRUE;

error:
	if (shell) {
		headless_shell_destroy(shell);
		free(shell);
	}
	return PEPPER_FALSE;
}
