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

#include <tbm_bufmgr.h>
#include <wayland-tbm-server.h>
#include <pepper-output-backend.h>
#include "HL_UI_LED.h"
#include "output_internal.h"

static const int KEY_OUTPUT;
static void led_output_add_frame_done(led_output_t *output);
static void led_output_update(led_output_t *output);

static void
led_output_destroy(void *data)
{
	led_output_t *output = (led_output_t *)data;
	PEPPER_TRACE("Output Destroy %p base %p\n", output, output->output);

	if (output->ui_led) {
		HL_UI_LED_Close(output->ui_led);
		output->ui_led = NULL;
	}

	if (output->tbm_server) {
		wayland_tbm_server_deinit(output->tbm_server);
		output->tbm_server = NULL;
	}
}

static int32_t
led_output_get_subpixel_order(void *o)
{
	return 0;
}

static const char *
led_output_get_maker_name(void *o)
{
	return "PePPer LED";
}

static const char *
led_output_get_model_name(void *o)
{
	return "PePPer LED";
}

static int
led_output_get_mode_count(void *o)
{
	return 1;
}

static void
led_output_get_mode(void *o, int index, pepper_output_mode_t *mode)
{
	led_output_t *output = (led_output_t *)o;

	PEPPER_TRACE("[OUTPUT]\n");

	if (index != 0)
		return;

	mode->flags = WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;
	mode->w = output->num_led;
	mode->h = output->num_led;
	mode->refresh = 60000;
}

static pepper_bool_t
led_output_set_mode(void *o, const pepper_output_mode_t *mode)
{
	return PEPPER_FALSE;
}

static void
led_output_assign_planes(void *o, const pepper_list_t *view_list)
{
	led_output_t *output = (led_output_t *)o;
	pepper_list_t *l;
	pepper_view_t *view, *top_view = NULL;

	PEPPER_TRACE("[OUTPUT] Assign plane\n");
	pepper_list_for_each_list(l, view_list) {
		view = (pepper_view_t*)l->item;

		if (pepper_view_is_mapped(view) && pepper_view_is_visible(view)) {
			top_view = view;
			break;
		}
	}

	if (output->top_view != top_view)
		PEPPER_TRACE("\tTop-View is changed(%p -> %p)\n", output->top_view, top_view);

	output->top_view = top_view;
}

static void
led_output_start_repaint_loop(void *o)
{
	led_output_t *output = (led_output_t *)o;
	struct timespec     ts;

	PEPPER_TRACE("[OUTPUT] Start reapint loop\n");
	pepper_compositor_get_time(output->compositor, &ts);
	pepper_output_finish_frame(output->output, &ts);
}

static void
led_output_repaint(void *o, const pepper_list_t *plane_list)
{
	pepper_list_t *l;
	pepper_plane_t *plane;
	led_output_t *output = (led_output_t *)o;

	PEPPER_TRACE("[OUTPUT] Repaint\n");

	pepper_list_for_each_list(l, plane_list) {
		plane = (pepper_plane_t *)l->item;
		pepper_plane_clear_damage_region(plane);
	}

	led_output_update(output);
	led_output_add_frame_done(output);
}

static void
led_output_attach_surface(void *o, pepper_surface_t *surface, int *w, int *h)
{
	*w = 10;
	*h = 10;
	PEPPER_TRACE("[OUTPUT] attach surface:%p\n", surface);
}

static void
led_output_flush_surface_damage(void *o, pepper_surface_t *surface, pepper_bool_t *keep_buffer)
{
	*keep_buffer = PEPPER_TRUE;
	PEPPER_TRACE("[OUTPUT] flush_surface_damage surface:%p\n", surface);
}

struct pepper_output_backend led_output_backend = {
	led_output_destroy,

	led_output_get_subpixel_order,
	led_output_get_maker_name,
	led_output_get_model_name,

	led_output_get_mode_count,
	led_output_get_mode,
	led_output_set_mode,

	led_output_assign_planes,
	led_output_start_repaint_loop,
	led_output_repaint,
	led_output_attach_surface,
	led_output_flush_surface_damage,
};

static void
led_output_update_led(led_output_t *output, unsigned char *data)
{
	int i;
	uint8_t *ptr = (uint8_t *)data;

	if (data == NULL) {
		PEPPER_TRACE("[OUTPUT] update LED to empty\n");
		HL_UI_LED_Clear_All(output->ui_led);
		return;
	}

	for(i=0; i<output->num_led; i++) {
		HL_UI_LED_Set_Pixel_RGB(output->ui_led, i, ptr[R_OFF_SET], ptr[G_OFF_SET], ptr[B_OFF_SET]);
		ptr += 4;
	}

	HL_UI_LED_Refresh(output->ui_led);
}

static void
led_output_update(led_output_t *output)
{
	pepper_buffer_t *buf;
	pepper_surface_t *surface;
	struct wl_resource *buf_res;
	tbm_surface_h tbm_surface;
	tbm_surface_info_s info;
	int ret;

	if (!output->top_view) {
		if (!output->ui_led)
			PEPPER_TRACE("[UPDATE LED] Empty Display\n");
		else
			led_output_update_led(output, NULL);

		return;
	}

	surface = pepper_view_get_surface(output->top_view);
	PEPPER_CHECK(surface, return, "fail to get a surafce from a view(%p)\n", output->top_view);

	buf = pepper_surface_get_buffer(surface);
	PEPPER_CHECK(buf, return, "fail to get a pepper_buffer from a surface(%p)\n", surface);

	buf_res = pepper_buffer_get_resource(buf);
	tbm_surface = wayland_tbm_server_get_surface(NULL, buf_res);
	PEPPER_CHECK(tbm_surface, return, "fail to get a tbm_surface from a pepper_buffer(%p)\n", buf);

	ret = tbm_surface_map(tbm_surface, TBM_SURF_OPTION_READ, &info);
	PEPPER_CHECK(ret == TBM_SURFACE_ERROR_NONE, return, "fail to map the tbm_surface\n");

	if (!output->ui_led)
		PEPPER_TRACE("[UPDATE LED] %s\n", (char*)info.planes[0].ptr);
	else
		led_output_update_led(output, info.planes[0].ptr);

	tbm_surface_unmap(tbm_surface);
}

static void
led_output_cb_frame_done(void *data)
{
	led_output_t *output = (led_output_t *)data;

	PEPPER_TRACE("[OUTPUT] frame_done %p\n", output);
	output->frame_done = NULL;

	pepper_output_finish_frame(output->output, NULL);
}

static void
led_output_add_frame_done(led_output_t *output)
{
	struct wl_event_loop *loop;

	PEPPER_TRACE("[OUTPUT] Add idle for frame(output:%p, frame_done:%p)\n", output, output->frame_done);

	if (!output || output->frame_done) {
		PEPPER_TRACE("[OUTPUT] skip add frame_done\n");
		return;
	}

	loop = wl_display_get_event_loop(pepper_compositor_get_display(output->compositor));
	PEPPER_CHECK(loop, return, "[OUTPUT] fail to get event loop\n");

	output->frame_done = wl_event_loop_add_idle(loop, led_output_cb_frame_done, output);
	PEPPER_CHECK(output->frame_done, return, "[OUTPUT] fail to add idle\n");
}

static void
pepper_output_bind_display(led_output_t *output)
{
	tbm_bufmgr bufmgr = NULL;

	PEPPER_CHECK(getenv("TBM_DISPLAY_SERVER"), return, "[TBM] run the subcompoitor mode\n");

	bufmgr = wayland_tbm_server_get_bufmgr(output->tbm_server);
	PEPPER_CHECK(bufmgr, return, "fail to get tbm_bufmgr\n");

	if (!tbm_bufmgr_bind_native_display(bufmgr, (void *)pepper_compositor_get_display(output->compositor)))
	{
		PEPPER_CHECK(0, return, "fail to tbm_bufmgr_bind_native_display\n");
	}

	return;
}

pepper_bool_t
headless_output_init(pepper_compositor_t *compositor)
{
	led_output_t *output = (led_output_t*)calloc(sizeof(led_output_t), 1);

	PEPPER_TRACE("Output Init\n");

	if (!output) {
		PEPPER_ERROR("Failed to allocate memory in %s\n", __FUNCTION__);
		goto error;
	}

	output->compositor = compositor;
	output->tbm_server = wayland_tbm_server_init(pepper_compositor_get_display(compositor), NULL, -1, 0);
	PEPPER_CHECK(output->tbm_server, goto error, "failed to wayland_tbm_server_init.\n");

	pepper_output_bind_display(output);

	output->num_led = NUM_LED;
	output->ui_led = HL_UI_LED_Init(output->num_led);
	if (output->ui_led) HL_UI_LED_Change_Brightness(output->ui_led, 0x1);

	if (!output->ui_led)
		PEPPER_ERROR("HL_UI_LED_Init() failed.\n");
	else
		boot_ani_start(output);

	output->output = pepper_compositor_add_output(compositor,
			&led_output_backend, "led_output",
			output,  WL_OUTPUT_TRANSFORM_NORMAL, 1);
	PEPPER_CHECK(output->output, goto error, "pepper_compositor_add_output() failed.\n");

	output->plane = pepper_output_add_plane(output->output, NULL);
	PEPPER_CHECK(output->plane, goto error, "pepper_output_add_plane() failed.\n");

	pepper_object_set_user_data((pepper_object_t *)compositor,
			&KEY_OUTPUT, output, NULL);
	PEPPER_TRACE("\t Add Output %p, base %p\n", output, output->output);
	PEPPER_TRACE("\t Add Output %p, plane %p\n", output, output->plane);
	PEPPER_TRACE("\t Userdata %p\n", pepper_object_get_user_data((pepper_object_t *)compositor,&KEY_OUTPUT));
	return PEPPER_TRUE;

error:
	if (output->ui_led)
		HL_UI_LED_Close(output->ui_led);

	if (output->tbm_server)
		wayland_tbm_server_deinit(output->tbm_server);

	if (output->output)
		pepper_output_destroy(output->output);

	if (output)
		free(output);
	return PEPPER_FALSE;
}

void
headless_output_deinit(pepper_compositor_t *compositor)
{
	led_output_t *output;


	output = pepper_object_get_user_data((pepper_object_t *)compositor, &KEY_OUTPUT);

	if (output) {
		pepper_object_set_user_data((pepper_object_t *)compositor, &KEY_OUTPUT, NULL, NULL);

		if (output->boot_ani) {
			boot_ani_stop(output);
		}

		pepper_output_destroy(output->output);
		led_output_destroy(output);

		free(output);
	}

	PEPPER_TRACE("Output Deinit ... DONE\n");
}
