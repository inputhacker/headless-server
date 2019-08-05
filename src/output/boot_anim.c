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

typedef struct {
	HL_UI_LED *led;

	struct wl_event_source *source;
	uint32_t serial;
	int index;

	pepper_event_listener_t *surface_add_listener;
} boot_ani_t;

#define ANI_INTERVAL	40	//20ms

static int
boot_ani_timer_cb(void *data)
{
	boot_ani_t *ani = (boot_ani_t *)data;
	uint8_t r,g,b;
	uint32_t color;

	if (ani->index == 0) {
		r = (uint8_t)(rand()%0xFF);
		g = (uint8_t)(rand()%0xFF);
		b = (uint8_t)(rand()%0xFF);
		HL_UI_LED_Set_Pixel_RGB(ani->led, ani->index, r, g, b);
	} else {
		color = HL_UI_LED_Get_Pixel_4byte(ani->led, ani->index - 1);
		HL_UI_LED_Set_Pixel_4byte(ani->led, ani->index, color);
	}

	HL_UI_LED_Refresh(ani->led);

	ani->serial++;
	ani->index = (ani->serial)%12;

	wl_event_source_timer_update(ani->source, ANI_INTERVAL);

	return 1;
}

static void
boot_ani_surface_add_cb(pepper_event_listener_t *listener,
										pepper_object_t *object,
										uint32_t id, void *info, void *data)
{
	led_output_t *output = (led_output_t *)data;

	boot_ani_stop(output);
}

void boot_ani_start(led_output_t *output)
{
	struct wl_event_loop *loop;
	boot_ani_t *ani;
	int ret;

	PEPPER_TRACE("[OUTPUT] start boot-animation\n");

	loop = wl_display_get_event_loop(pepper_compositor_get_display(output->compositor));
	PEPPER_CHECK(loop, return, "failed to wl_display_get_event_loop()\n");

	ani = (boot_ani_t *)calloc(sizeof(boot_ani_t), 1);
	PEPPER_CHECK(ani, return, "failed to alloc\n");

	ani->source = wl_event_loop_add_timer(loop, boot_ani_timer_cb, ani);
	PEPPER_CHECK(ani, goto err, "failed to wl_event_loop_add_timer()\n");

	ret = wl_event_source_timer_update(ani->source, ANI_INTERVAL);
	PEPPER_CHECK(!ret, goto err, "failed to wl_event_source_timer_update\n");

	ani->surface_add_listener = pepper_object_add_event_listener((pepper_object_t *)output->compositor,
																		PEPPER_EVENT_COMPOSITOR_SURFACE_ADD,
																		0, boot_ani_surface_add_cb, output);

	ani->led = output->ui_led;
	output->boot_ani = ani;
	return;
err:
	if (ani) {
		if (ani->source)
			wl_event_source_remove(ani->source);

		free(ani);
	}
	return;
}

void boot_ani_stop(led_output_t *output)
{
	boot_ani_t *ani;

	if (!output->boot_ani) return;

	ani = (boot_ani_t *)output->boot_ani;

	HL_UI_LED_Clear_All(ani->led);
	wl_event_source_remove(ani->source);

	if(ani->surface_add_listener) {
		pepper_event_listener_remove(ani->surface_add_listener);
		ani->surface_add_listener = NULL;
	}

	free(ani);

	output->boot_ani = NULL;
	return;
}
