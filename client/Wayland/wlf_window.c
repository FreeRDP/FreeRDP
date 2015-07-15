/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Wayland Windows
 *
 * Copyright 2014 Manuel Bachmann <tarnyko@tarnyko.net>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "wlf_window.h"

static void wl_shell_surface_handle_ping(void* data, struct wl_shell_surface* shell_surface, uint32_t serial)
{
	wl_shell_surface_pong(shell_surface, serial);
}

static void wl_shell_surface_handle_configure(void* data, struct wl_shell_surface* shell_surface, unsigned int edges, int32_t width, int32_t height)
{
	wlfWindow* window = data;

	window->width = width;
	window->height = height;
}

static const struct wl_shell_surface_listener wl_shell_surface_listener =
{
	wl_shell_surface_handle_ping,
	wl_shell_surface_handle_configure,
	NULL
};

static void wl_buffer_release(void* data, struct wl_buffer* wl_buffer)
{
	wlfBuffer* buffer = data;

	buffer->busy = FALSE;
}

static const struct wl_buffer_listener wl_buffer_listener =
{
	wl_buffer_release
};

static const struct wl_callback_listener wl_callback_listener;

static void wl_callback_done(void* data, struct wl_callback* callback, uint32_t time)
{
	wlfWindow* window = data;
	wlfBuffer* buffer;
	struct wl_shm_pool* shm_pool;
	void* shm_data;
	void* free_data;
	int fd;
	int fdt;

	if (!window->buffers[0].busy)
		buffer = &window->buffers[0];
	else if (!window->buffers[1].busy)
		buffer = &window->buffers[1];
	else
		return;

	if (!buffer->buffer) {
		fd = shm_open("/wlfreerdp_shm", O_CREAT | O_RDWR, 0666);
		fdt = ftruncate(fd, window->width * window->height * 4);
		if (fdt != 0)
		{
			WLog_ERR(TAG, "window_redraw: could not allocate memory");
			close(fd);
			return;
		}

		shm_data = mmap(NULL, window->width * window->height * 4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (shm_data == MAP_FAILED)
		{
			WLog_ERR(TAG, "window_redraw: failed to memory map buffer");
			close(fd);
			return;
		}

		shm_pool = wl_shm_create_pool(window->display->shm, fd, window->width * window->height * 4);
		buffer->buffer = wl_shm_pool_create_buffer(shm_pool, 0, window->width, window->height, window->width* 4, WL_SHM_FORMAT_XRGB8888);
		wl_buffer_add_listener(buffer->buffer, &wl_buffer_listener, buffer);
		wl_shm_pool_destroy(shm_pool);
		shm_unlink("/wlfreerdp_shm");
		close(fd);

		free_data = buffer->shm_data;
		buffer->shm_data = shm_data;
		munmap(free_data, window->width * window->height * 4);
	}

	 /* this is the real surface data */
	memcpy(buffer->shm_data, (void*) window->data, window->width * window->height * 4);
	wl_surface_attach(window->surface, buffer->buffer, 0, 0);
	wl_surface_damage(window->surface, 0, 0, window->width, window->height);

	if (callback) wl_callback_destroy(callback);
	window->callback = wl_surface_frame(window->surface);
	wl_callback_add_listener(window->callback, &wl_callback_listener, window);
	wl_surface_commit(window->surface);

	buffer->busy = TRUE;
}

static const struct wl_callback_listener wl_callback_listener =
{
	wl_callback_done
};


wlfWindow* wlf_CreateDesktopWindow(wlfContext* wlfc, char* name, int width, int height, BOOL decorations)
{
	wlfWindow* window;

	window = (wlfWindow*) calloc(1, sizeof(wlfWindow));

	if (window)
	{
		window->width = width;
		window->height = height;
		window->fullscreen = FALSE;
		window->buffers[0].busy = FALSE;
		window->buffers[1].busy = FALSE;
		window->callback = NULL;
		window->display = wlfc->display;

		window->surface = wl_compositor_create_surface(window->display->compositor);
		window->shell_surface = wl_shell_get_shell_surface(window->display->shell, window->surface);
		wl_shell_surface_add_listener(window->shell_surface, &wl_shell_surface_listener, window);
		wl_shell_surface_set_toplevel(window->shell_surface);

		wlf_ResizeDesktopWindow(wlfc, window, width, height);

		wl_surface_damage(window->surface, 0, 0, window->width, window->height);

		wlf_SetWindowText(wlfc, window, name);
	}

	return window;
}

void wlf_ResizeDesktopWindow(wlfContext* wlfc, wlfWindow* window, int width, int height)
{
	window->width = width;
	window->height = height;
}

void wlf_SetWindowText(wlfContext* wlfc, wlfWindow* window, char* name)
{
	wl_shell_surface_set_title(window->shell_surface, name);
}

void wlf_SetWindowFullscreen(wlfContext* wlfc, wlfWindow* window, BOOL fullscreen)
{
	if (fullscreen)
	{
		wl_shell_surface_set_fullscreen(window->shell_surface, WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT, 0, NULL);
		window->fullscreen = TRUE;
	}
}

void wlf_ShowWindow(wlfContext* wlfc, wlfWindow* window, BYTE state)
{
	switch (state)
	{
		case WINDOW_HIDE:
		case WINDOW_SHOW_MINIMIZED:
			/* xdg_surface_set_minimized(window->xdg_surface); */
			break;
		case WINDOW_SHOW_MAXIMIZED:
			wl_shell_surface_set_maximized(window->shell_surface, NULL);
			break;
		case WINDOW_SHOW:
			wl_shell_surface_set_toplevel(window->shell_surface);
			break;
	}
}

void wlf_UpdateWindowArea(wlfContext* wlfc, wlfWindow* window, int x, int y, int width, int height)
{
	wl_callback_done(window, NULL, 0);
}

void wlf_DestroyWindow(wlfContext* wlfc, wlfWindow* window)
{
	if (window == NULL)
		return;

	if (wlfc->window == window)
		wlfc->window = NULL;

	if (window->buffers[0].buffer)
		wl_buffer_destroy(window->buffers[0].buffer);
	if (window->buffers[1].buffer)
		wl_buffer_destroy(window->buffers[1].buffer);
	if (window->shell_surface)
		wl_shell_surface_destroy(window->shell_surface);
	if (window->surface)
		wl_surface_destroy(window->surface);

	free(window->data);
	free(window);
}
