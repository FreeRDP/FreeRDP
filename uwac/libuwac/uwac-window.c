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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>

#include "uwac-priv.h"
#include "uwac-utils.h"
#include "uwac-os.h"

#define UWAC_INITIAL_BUFFERS 3

static int bppFromShmFormat(enum wl_shm_format format)
{
	switch (format)
	{
		case WL_SHM_FORMAT_ARGB8888:
		case WL_SHM_FORMAT_XRGB8888:
		default:
			return 4;
	}
}

static void buffer_release(void* data, struct wl_buffer* buffer)
{
	UwacBuffer* uwacBuffer = (UwacBuffer*)data;
	uwacBuffer->used = false;
}

static const struct wl_buffer_listener buffer_listener = { buffer_release };

static void UwacWindowDestroyBuffers(UwacWindow* w)
{
	int i;

	for (i = 0; i < w->nbuffers; i++)
	{
		UwacBuffer* buffer = &w->buffers[i];
#ifdef HAVE_PIXMAN_REGION
		pixman_region32_fini(&buffer->damage);
#else
		region16_uninit(&buffer->damage);
#endif
		wl_buffer_destroy(buffer->wayland_buffer);
	}

	w->nbuffers = 0;
	free(w->buffers);
	w->buffers = NULL;
}

static int UwacWindowShmAllocBuffers(UwacWindow* w, int nbuffers, int allocSize, uint32_t width,
                                     uint32_t height, enum wl_shm_format format);

static void xdg_handle_toplevel_configure(void* data, struct xdg_toplevel* xdg_toplevel,
                                          int32_t width, int32_t height, struct wl_array* states)
{
	UwacWindow* window = (UwacWindow*)data;
	UwacConfigureEvent* event;
	int ret, surfaceState;
	enum xdg_toplevel_state* state;
	surfaceState = 0;
	wl_array_for_each(state, states)
	{
		switch (*state)
		{
			case XDG_TOPLEVEL_STATE_MAXIMIZED:
				surfaceState |= UWAC_WINDOW_MAXIMIZED;
				break;

			case XDG_TOPLEVEL_STATE_FULLSCREEN:
				surfaceState |= UWAC_WINDOW_FULLSCREEN;
				break;

			case XDG_TOPLEVEL_STATE_ACTIVATED:
				surfaceState |= UWAC_WINDOW_ACTIVATED;
				break;

			case XDG_TOPLEVEL_STATE_RESIZING:
				surfaceState |= UWAC_WINDOW_RESIZING;
				break;

			default:
				break;
		}
	}
	window->surfaceStates = surfaceState;
	event = (UwacConfigureEvent*)UwacDisplayNewEvent(window->display, UWAC_EVENT_CONFIGURE);

	if (!event)
	{
		assert(uwacErrorHandler(window->display, UWAC_ERROR_NOMEMORY,
		                        "failed to allocate a configure event\n"));
		return;
	}

	event->window = window;
	event->states = surfaceState;

	if (width && height)
	{
		event->width = width;
		event->height = height;
		UwacWindowDestroyBuffers(window);
		window->width = width;
		window->stride = width * bppFromShmFormat(window->format);
		window->height = height;
		ret = UwacWindowShmAllocBuffers(window, UWAC_INITIAL_BUFFERS, window->stride * height,
		                                width, height, window->format);

		if (ret != UWAC_SUCCESS)
		{
			assert(
			    uwacErrorHandler(window->display, ret, "failed to reallocate a wayland buffers\n"));
			window->drawingBuffer = window->pendingBuffer = NULL;
			return;
		}

		window->drawingBuffer = &window->buffers[0];
		if (window->pendingBuffer != NULL)
			window->pendingBuffer = window->drawingBuffer;
	}
	else
	{
		event->width = window->width;
		event->height = window->height;
	}
}

static void xdg_handle_toplevel_close(void* data, struct xdg_toplevel* xdg_toplevel)
{
	UwacCloseEvent* event;
	UwacWindow* window = (UwacWindow*)data;
	event = (UwacCloseEvent*)UwacDisplayNewEvent(window->display, UWAC_EVENT_CLOSE);

	if (!event)
	{
		assert(uwacErrorHandler(window->display, UWAC_ERROR_INTERNAL,
		                        "failed to allocate a close event\n"));
		return;
	}

	event->window = window;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	xdg_handle_toplevel_configure,
	xdg_handle_toplevel_close,
};

static void xdg_handle_surface_configure(void* data, struct xdg_surface* xdg_surface,
                                         uint32_t serial)
{
	xdg_surface_ack_configure(xdg_surface, serial);
	UwacWindow* window = (UwacWindow*)data;
	wl_surface_commit(window->surface);
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_handle_surface_configure,
};

#if BUILD_IVI

static void ivi_handle_configure(void* data, struct ivi_surface* surface, int32_t width,
                                 int32_t height)
{
	UwacWindow* window = (UwacWindow*)data;
	UwacConfigureEvent* event;
	int ret;
	event = (UwacConfigureEvent*)UwacDisplayNewEvent(window->display, UWAC_EVENT_CONFIGURE);

	if (!event)
	{
		assert(uwacErrorHandler(window->display, UWAC_ERROR_NOMEMORY,
		                        "failed to allocate a configure event\n"));
		return;
	}

	event->window = window;
	event->states = 0;

	if (width && height)
	{
		event->width = width;
		event->height = height;
		UwacWindowDestroyBuffers(window);
		window->width = width;
		window->stride = width * bppFromShmFormat(window->format);
		window->height = height;
		ret = UwacWindowShmAllocBuffers(window, UWAC_INITIAL_BUFFERS, window->stride * height,
		                                width, height, window->format);

		if (ret != UWAC_SUCCESS)
		{
			assert(
			    uwacErrorHandler(window->display, ret, "failed to reallocate a wayland buffers\n"));
			window->drawingBuffer = window->pendingBuffer = NULL;
			return;
		}

		window->drawingBuffer = &window->buffers[0];
		if (window->pendingBuffer != NULL)
			window->pendingBuffer = window->drawingBuffer;
	}
	else
	{
		event->width = window->width;
		event->height = window->height;
	}
}

static const struct ivi_surface_listener ivi_surface_listener = {
	ivi_handle_configure,
};
#endif

static void shell_ping(void* data, struct wl_shell_surface* surface, uint32_t serial)
{
	wl_shell_surface_pong(surface, serial);
}

static void shell_configure(void* data, struct wl_shell_surface* surface, uint32_t edges,
                            int32_t width, int32_t height)
{
	UwacWindow* window = (UwacWindow*)data;
	UwacConfigureEvent* event;
	int ret;
	event = (UwacConfigureEvent*)UwacDisplayNewEvent(window->display, UWAC_EVENT_CONFIGURE);

	if (!event)
	{
		assert(uwacErrorHandler(window->display, UWAC_ERROR_NOMEMORY,
		                        "failed to allocate a configure event\n"));
		return;
	}

	event->window = window;
	event->states = 0;

	if (width && height)
	{
		event->width = width;
		event->height = height;
		UwacWindowDestroyBuffers(window);
		window->width = width;
		window->stride = width * bppFromShmFormat(window->format);
		window->height = height;
		ret = UwacWindowShmAllocBuffers(window, UWAC_INITIAL_BUFFERS, window->stride * height,
		                                width, height, window->format);

		if (ret != UWAC_SUCCESS)
		{
			assert(
			    uwacErrorHandler(window->display, ret, "failed to reallocate a wayland buffers\n"));
			window->drawingBuffer = window->pendingBuffer = NULL;
			return;
		}

		window->drawingBuffer = &window->buffers[0];
		if (window->pendingBuffer != NULL)
			window->pendingBuffer = window->drawingBuffer;
	}
	else
	{
		event->width = window->width;
		event->height = window->height;
	}
}

static void shell_popup_done(void* data, struct wl_shell_surface* surface)
{
}

static const struct wl_shell_surface_listener shell_listener = { shell_ping, shell_configure,
	                                                             shell_popup_done };

int UwacWindowShmAllocBuffers(UwacWindow* w, int nbuffers, int allocSize, uint32_t width,
                              uint32_t height, enum wl_shm_format format)
{
	int ret = UWAC_SUCCESS;
	UwacBuffer* newBuffers;
	int i, fd;
	void* data;
	struct wl_shm_pool* pool;
	newBuffers = xrealloc(w->buffers, (w->nbuffers + nbuffers) * sizeof(UwacBuffer));

	if (!newBuffers)
		return UWAC_ERROR_NOMEMORY;

	w->buffers = newBuffers;
	memset(w->buffers + w->nbuffers, 0, sizeof(UwacBuffer) * nbuffers);
	fd = uwac_create_anonymous_file(allocSize * nbuffers);

	if (fd < 0)
	{
		return UWAC_ERROR_INTERNAL;
	}

	data = mmap(NULL, allocSize * nbuffers, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (data == MAP_FAILED)
	{
		ret = UWAC_ERROR_NOMEMORY;
		goto error_mmap;
	}

	pool = wl_shm_create_pool(w->display->shm, fd, allocSize * nbuffers);

	if (!pool)
	{
		munmap(data, allocSize * nbuffers);
		ret = UWAC_ERROR_NOMEMORY;
		goto error_mmap;
	}

	for (i = 0; i < nbuffers; i++)
	{
		UwacBuffer* buffer = &w->buffers[w->nbuffers + i];
#ifdef HAVE_PIXMAN_REGION
		pixman_region32_init(&buffer->damage);
#else
		region16_init(&buffer->damage);
#endif
		buffer->data = data + (allocSize * i);
		buffer->wayland_buffer =
		    wl_shm_pool_create_buffer(pool, allocSize * i, width, height, w->stride, format);
		wl_buffer_add_listener(buffer->wayland_buffer, &buffer_listener, buffer);
	}

	wl_shm_pool_destroy(pool);
	w->nbuffers += nbuffers;
error_mmap:
	close(fd);
	return ret;
}

static UwacBuffer* UwacWindowFindFreeBuffer(UwacWindow* w)
{
	int i, ret;

	for (i = 0; i < w->nbuffers; i++)
	{
		if (!w->buffers[i].used)
		{
			w->buffers[i].used = true;
			return &w->buffers[i];
		}
	}

	ret = UwacWindowShmAllocBuffers(w, 2, w->stride * w->height, w->width, w->height, w->format);

	if (ret != UWAC_SUCCESS)
	{
		w->display->last_error = ret;
		return NULL;
	}

	w->buffers[i].used = true;
	return &w->buffers[i];
}

static UwacReturnCode UwacWindowSetDecorations(UwacWindow* w)
{
	if (!w || !w->display)
		return UWAC_ERROR_INTERNAL;

	if (w->display->deco_manager)
	{
		w->deco = zxdg_decoration_manager_v1_get_toplevel_decoration(w->display->deco_manager,
		                                                             w->xdg_toplevel);
		if (!w->deco)
		{
			uwacErrorHandler(w->display, UWAC_NOT_FOUND,
			                 "Current window manager does not allow decorating with SSD");
		}
		else
			zxdg_toplevel_decoration_v1_set_mode(w->deco,
			                                     ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}
	else if (w->display->kde_deco_manager)
	{
		w->kde_deco =
		    org_kde_kwin_server_decoration_manager_create(w->display->kde_deco_manager, w->surface);
		if (!w->kde_deco)
		{
			uwacErrorHandler(w->display, UWAC_NOT_FOUND,
			                 "Current window manager does not allow decorating with SSD");
		}
		else
			org_kde_kwin_server_decoration_request_mode(w->kde_deco,
			                                            ORG_KDE_KWIN_SERVER_DECORATION_MODE_SERVER);
	}
	return UWAC_SUCCESS;
}

UwacWindow* UwacCreateWindowShm(UwacDisplay* display, uint32_t width, uint32_t height,
                                enum wl_shm_format format)
{
	UwacWindow* w;
	int allocSize, ret;

	if (!display)
	{
		return NULL;
	}

	w = xzalloc(sizeof(*w));

	if (!w)
	{
		display->last_error = UWAC_ERROR_NOMEMORY;
		return NULL;
	}

	w->display = display;
	w->format = format;
	w->width = width;
	w->height = height;
	w->stride = width * bppFromShmFormat(format);
	allocSize = w->stride * height;
	ret = UwacWindowShmAllocBuffers(w, UWAC_INITIAL_BUFFERS, allocSize, width, height, format);

	if (ret != UWAC_SUCCESS)
	{
		display->last_error = ret;
		goto out_error_free;
	}

	w->buffers[0].used = true;
	w->drawingBuffer = &w->buffers[0];
	w->surface = wl_compositor_create_surface(display->compositor);

	if (!w->surface)
	{
		display->last_error = UWAC_ERROR_NOMEMORY;
		goto out_error_surface;
	}

	wl_surface_set_user_data(w->surface, w);

	if (display->xdg_base)
	{
		w->xdg_surface = xdg_wm_base_get_xdg_surface(display->xdg_base, w->surface);

		if (!w->xdg_surface)
		{
			display->last_error = UWAC_ERROR_NOMEMORY;
			goto out_error_shell;
		}

		xdg_surface_add_listener(w->xdg_surface, &xdg_surface_listener, w);

		w->xdg_toplevel = xdg_surface_get_toplevel(w->xdg_surface);
		if (!w->xdg_toplevel)
		{
			display->last_error = UWAC_ERROR_NOMEMORY;
			goto out_error_shell;
		}

		assert(w->xdg_surface);
		xdg_toplevel_add_listener(w->xdg_toplevel, &xdg_toplevel_listener, w);
		wl_surface_commit(w->surface);
		wl_display_roundtrip(w->display->display);
	}
#if BUILD_IVI
	else if (display->ivi_application)
	{
		w->ivi_surface = ivi_application_surface_create(display->ivi_application, 1, w->surface);
		assert(w->ivi_surface);
		ivi_surface_add_listener(w->ivi_surface, &ivi_surface_listener, w);
	}
#endif
#if BUILD_FULLSCREEN_SHELL
	else if (display->fullscreen_shell)
	{
		zwp_fullscreen_shell_v1_present_surface(display->fullscreen_shell, w->surface,
		                                        ZWP_FULLSCREEN_SHELL_V1_PRESENT_METHOD_CENTER,
		                                        NULL);
	}
#endif
	else
	{
		w->shell_surface = wl_shell_get_shell_surface(display->shell, w->surface);
		assert(w->shell_surface);
		wl_shell_surface_add_listener(w->shell_surface, &shell_listener, w);
		wl_shell_surface_set_toplevel(w->shell_surface);
	}

	wl_list_insert(display->windows.prev, &w->link);
	display->last_error = UWAC_SUCCESS;
	UwacWindowSetDecorations(w);
	return w;
out_error_shell:
	wl_surface_destroy(w->surface);
out_error_surface:
	UwacWindowDestroyBuffers(w);
out_error_free:
	free(w);
	return NULL;
}

UwacReturnCode UwacDestroyWindow(UwacWindow** pwindow)
{
	UwacWindow* w;
	assert(pwindow);
	w = *pwindow;
	UwacWindowDestroyBuffers(w);

	if (w->deco)
		zxdg_toplevel_decoration_v1_destroy(w->deco);

	if (w->kde_deco)
		org_kde_kwin_server_decoration_destroy(w->kde_deco);

	if (w->xdg_surface)
		xdg_surface_destroy(w->xdg_surface);

#if BUILD_IVI

	if (w->ivi_surface)
		ivi_surface_destroy(w->ivi_surface);

#endif

	if (w->opaque_region)
		wl_region_destroy(w->opaque_region);

	if (w->input_region)
		wl_region_destroy(w->input_region);

	wl_surface_destroy(w->surface);
	wl_list_remove(&w->link);
	free(w);
	*pwindow = NULL;
	return UWAC_SUCCESS;
}

UwacReturnCode UwacWindowSetOpaqueRegion(UwacWindow* window, uint32_t x, uint32_t y, uint32_t width,
                                         uint32_t height)
{
	assert(window);

	if (window->opaque_region)
		wl_region_destroy(window->opaque_region);

	window->opaque_region = wl_compositor_create_region(window->display->compositor);

	if (!window->opaque_region)
		return UWAC_ERROR_NOMEMORY;

	wl_region_add(window->opaque_region, x, y, width, height);
	wl_surface_set_opaque_region(window->surface, window->opaque_region);
	return UWAC_SUCCESS;
}

UwacReturnCode UwacWindowSetInputRegion(UwacWindow* window, uint32_t x, uint32_t y, uint32_t width,
                                        uint32_t height)
{
	assert(window);

	if (window->input_region)
		wl_region_destroy(window->input_region);

	window->input_region = wl_compositor_create_region(window->display->compositor);

	if (!window->input_region)
		return UWAC_ERROR_NOMEMORY;

	wl_region_add(window->input_region, x, y, width, height);
	wl_surface_set_input_region(window->surface, window->input_region);
	return UWAC_SUCCESS;
}

void* UwacWindowGetDrawingBuffer(UwacWindow* window)
{
	return window->drawingBuffer->data;
}

static void frame_done_cb(void* data, struct wl_callback* callback, uint32_t time);

static const struct wl_callback_listener frame_listener = { frame_done_cb };

#ifdef HAVE_PIXMAN_REGION
static void damage_surface(UwacWindow* window, UwacBuffer* buffer)
{
	UINT32 nrects, i;
	const pixman_box32_t* box = pixman_region32_rectangles(&buffer->damage, &nrects);

	for (i = 0; i < nrects; i++, box++)
		wl_surface_damage(window->surface, box->x1, box->y1, (box->x2 - box->x1),
		                  (box->y2 - box->y1));

	pixman_region32_clear(&buffer->damage);
}
#else
static void damage_surface(UwacWindow* window, UwacBuffer* buffer)
{
	UINT32 nrects, i;
	const RECTANGLE_16* box = region16_rects(&buffer->damage, &nrects);

	for (i = 0; i < nrects; i++, box++)
		wl_surface_damage(window->surface, box->left, box->top, (box->right - box->left),
		                  (box->bottom - box->top));

	region16_clear(&buffer->damage);
}
#endif

static void UwacSubmitBufferPtr(UwacWindow* window, UwacBuffer* buffer)
{
	wl_surface_attach(window->surface, buffer->wayland_buffer, 0, 0);

	damage_surface(window, buffer);

	struct wl_callback* frame_callback = wl_surface_frame(window->surface);
	wl_callback_add_listener(frame_callback, &frame_listener, window);
	wl_surface_commit(window->surface);
	buffer->dirty = false;
}

static void frame_done_cb(void* data, struct wl_callback* callback, uint32_t time)
{
	UwacWindow* window = (UwacWindow*)data;
	UwacFrameDoneEvent* event;

	wl_callback_destroy(callback);
	window->pendingBuffer = NULL;
	event = (UwacFrameDoneEvent*)UwacDisplayNewEvent(window->display, UWAC_EVENT_FRAME_DONE);

	if (event)
		event->window = window;
}

#ifdef HAVE_PIXMAN_REGION
UwacReturnCode UwacWindowAddDamage(UwacWindow* window, uint32_t x, uint32_t y, uint32_t width,
                                   uint32_t height)
{
	UwacBuffer* buf = window->drawingBuffer;
	if (!pixman_region32_union_rect(&buf->damage, &buf->damage, x, y, width, height))
		return UWAC_ERROR_INTERNAL;

	buf->dirty = true;
	return UWAC_SUCCESS;
}
#else
UwacReturnCode UwacWindowAddDamage(UwacWindow* window, uint32_t x, uint32_t y, uint32_t width,
                                   uint32_t height)
{
	RECTANGLE_16 box;

	box.left = x;
	box.top = y;
	box.right = x + width;
	box.bottom = y + height;

	UwacBuffer* buf = window->drawingBuffer;
	if (!region16_union_rect(&buf->damage, &buf->damage, &box))
		return UWAC_ERROR_INTERNAL;

	buf->dirty = true;
	return UWAC_SUCCESS;
}
#endif

UwacReturnCode UwacWindowGetDrawingBufferGeometry(UwacWindow* window, UwacSize* geometry,
                                                  size_t* stride)
{
	if (!window || !window->drawingBuffer)
		return UWAC_ERROR_INTERNAL;

	if (geometry)
	{
		geometry->width = window->width;
		geometry->height = window->height;
	}

	if (stride)
		*stride = window->stride;

	return UWAC_SUCCESS;
}

UwacReturnCode UwacWindowSubmitBuffer(UwacWindow* window, bool copyContentForNextFrame)
{
	UwacBuffer* drawingBuffer = window->drawingBuffer;

	if (window->pendingBuffer || !drawingBuffer->dirty)
		return UWAC_SUCCESS;

	window->pendingBuffer = drawingBuffer;
	window->drawingBuffer = UwacWindowFindFreeBuffer(window);

	if (!window->drawingBuffer)
		return UWAC_ERROR_NOMEMORY;

	if (copyContentForNextFrame)
		memcpy(window->drawingBuffer->data, window->pendingBuffer->data,
		       window->stride * window->height);

	UwacSubmitBufferPtr(window, drawingBuffer);
	return UWAC_SUCCESS;
}

UwacReturnCode UwacWindowGetGeometry(UwacWindow* window, UwacSize* geometry)
{
	assert(window);
	assert(geometry);
	geometry->width = window->width;
	geometry->height = window->height;
	return UWAC_SUCCESS;
}

UwacReturnCode UwacWindowSetFullscreenState(UwacWindow* window, UwacOutput* output,
                                            bool isFullscreen)
{
	if (window->xdg_toplevel)
	{
		if (isFullscreen)
		{
			xdg_toplevel_set_fullscreen(window->xdg_toplevel, output ? output->output : NULL);
		}
		else
		{
			xdg_toplevel_unset_fullscreen(window->xdg_toplevel);
		}
	}
	else if (window->shell_surface)
	{
		if (isFullscreen)
		{
			wl_shell_surface_set_fullscreen(window->shell_surface,
			                                WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT, 0,
			                                output ? output->output : NULL);
		}
		else
		{
			wl_shell_surface_set_toplevel(window->shell_surface);
		}
	}

	return UWAC_SUCCESS;
}

void UwacWindowSetTitle(UwacWindow* window, const char* name)
{
	if (window->xdg_toplevel)
		xdg_toplevel_set_title(window->xdg_toplevel, name);
	else if (window->shell_surface)
		wl_shell_surface_set_title(window->shell_surface, name);
}
