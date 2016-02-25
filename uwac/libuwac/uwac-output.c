/*
 * Copyright © 2014 David FORT <contact@hardening-consulting.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */
#include "uwac-priv.h"
#include "uwac-utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TARGET_OUTPUT_INTERFACE 2

static void output_handle_geometry(void *data, struct wl_output *wl_output, int x, int y,
			int physical_width, int physical_height, int subpixel,
			const char *make, const char *model, int transform)
{
	UwacOutput *output = data;

/*	output->allocation.x = x;
	output->allocation.y = y;*/
	output->transform = transform;

	if (output->make)
		free(output->make);
	output->make = strdup(make);
	if (!output->make) {
		assert(uwacErrorHandler(output->display, UWAC_ERROR_NOMEMORY, "%s: unable to strdup make\n", __FUNCTION__));
	}

	if (output->model)
		free(output->model);
	output->model = strdup(model);
	if (!output->model) {
		assert(uwacErrorHandler(output->display, UWAC_ERROR_NOMEMORY, "%s: unable to strdup model\n", __FUNCTION__));
	}
}

static void output_handle_done(void *data, struct wl_output *wl_output)
{
	UwacOutput *output = data;

	output->doneReceived = true;
}

static void output_handle_scale(void *data, struct wl_output *wl_output, int32_t scale)
{
	UwacOutput *output = data;

	output->scale = scale;
}

static void output_handle_mode(void *data, struct wl_output *wl_output, uint32_t flags,
		    int width, int height, int refresh)
{
	UwacOutput *output = data;
	//UwacDisplay *display = output->display;

	if (output->doneNeeded && output->doneReceived) {
		/* TODO: we should clear the mode list */
	}

	if (flags & WL_OUTPUT_MODE_CURRENT) {
		output->resolution.width = width;
		output->resolution.height = height;
	/*	output->allocation.width = width;
		output->allocation.height = height;
		if (display->output_configure_handler)
			(*display->output_configure_handler)(
						output, display->user_data);*/
	}
}

static const struct wl_output_listener output_listener = {
	output_handle_geometry,
	output_handle_mode,
	output_handle_done,
	output_handle_scale
};

UwacOutput *UwacCreateOutput(UwacDisplay *d, uint32_t id, uint32_t version) {
	UwacOutput *o;

	o = zalloc(sizeof *o);
	if (!o)
		return NULL;

	o->display = d;
	o->server_output_id = id;
	o->doneNeeded = (version > 1);
	o->doneReceived = false;
	o->output = wl_registry_bind(d->registry, id, &wl_output_interface, min(TARGET_OUTPUT_INTERFACE, version));
	wl_output_add_listener(o->output, &output_listener, o);

	wl_list_insert(d->outputs.prev, &o->link);
	return o;
}

int UwacDestroyOutput(UwacOutput *output) {
	free(output->make);
	free(output->model);

	wl_output_destroy(output->output);
	wl_list_remove(&output->link);
	free(output);

	return UWAC_SUCCESS;
}
