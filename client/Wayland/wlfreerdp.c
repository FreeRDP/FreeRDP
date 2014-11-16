/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Wayland Client Interface
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
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <freerdp/log.h>
#include <freerdp/freerdp.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/channels/channels.h>
#include <freerdp/gdi/gdi.h>

#include <wayland-client.h>

#define TAG CLIENT_TAG("wayland")

struct display
{
	struct wl_display* display;
	struct wl_registry* registry;
	struct wl_compositor* compositor;
	struct wl_shell* shell;
	struct wl_shm* shm;
};

struct buffer
{
	struct wl_buffer* buffer;
	void* shm_data;
	int busy;
};

struct window
{
	int width, height;
	struct wl_surface* surface;
	struct wl_shell_surface* shell_surface;
	struct wl_callback* callback;
	struct buffer buffers[2];
	struct display* display;
	void* data;
};

struct wl_context
{
	rdpContext _p;
	struct display* display;
	struct window* window;
};

static void wl_buffer_release(void* data, struct wl_buffer* wl_buffer)
{
	struct buffer* buffer = data;

	buffer->busy = 0;
}

static const struct wl_buffer_listener wl_buffer_listener =
{
	wl_buffer_release
};

static void window_redraw(void* data, struct wl_callback* callback, uint32_t time);
static const struct wl_callback_listener wl_callback_listener = 
{
	window_redraw
};

static void window_redraw(void* data, struct wl_callback* callback, uint32_t time)
{
	struct window* window = data;
	struct wl_shm_pool* shm_pool;
	struct buffer* buffer;
	int fd;
	int fdt;

	if (!window->buffers[0].busy)
		buffer = &window->buffers[0];
	else if (!window->buffers[1].busy)
		buffer = &window->buffers[1];
	else
		return;

	fd = shm_open("wlfreerdp_shm", O_CREAT | O_TRUNC | O_RDWR, 0666);
	fdt = ftruncate(fd, window->width * window->height * 4);
	if (fdt != 0)
	{
		WLog_ERR(TAG, "window_redraw: could not allocate memory");
		close(fd);
		return;
	}

	buffer->shm_data = mmap(0, window->width * window->height * 4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (buffer->shm_data == MAP_FAILED)
	{
		WLog_ERR(TAG, "window_redraw: failed to memory map buffer");
		close(fd);
		return;
	}

	shm_pool = wl_shm_create_pool(window->display->shm, fd, window->width * window->height * 4);
	buffer->buffer = wl_shm_pool_create_buffer(shm_pool, 0, window->width, window->height, window->width* 4, WL_SHM_FORMAT_XRGB8888);
	wl_buffer_add_listener(buffer->buffer, &wl_buffer_listener, buffer);
	wl_shm_pool_destroy(shm_pool);
	shm_unlink("wlfreerdp_shm");
	close(fd);

	 /* this is the real surface data */
	memcpy(buffer->shm_data, (void*) window->data, window->width * window->height * 4);
	wl_surface_attach(window->surface, buffer->buffer, 0, 0);
	wl_surface_damage(window->surface, 0, 0, window->width, window->height);

	if (callback) wl_callback_destroy(callback);
	window->callback = wl_surface_frame(window->surface);
	wl_callback_add_listener(window->callback, &wl_callback_listener, window);
	wl_surface_commit(window->surface);

	buffer->busy = 1;
	munmap(buffer->shm_data, window->width * window->height * 4);
}

static void wl_shell_surface_handle_ping(void* data, struct wl_shell_surface* shell_surface, uint32_t serial)
{
	wl_shell_surface_pong(shell_surface, serial);
}

static const struct wl_shell_surface_listener wl_shell_surface_listener =
{
	wl_shell_surface_handle_ping
};

static void wl_registry_handle_global(void* data, struct wl_registry* registry, uint32_t id, const char *interface, uint32_t version)
{
	struct display* display = data;

	if (strcmp(interface, "wl_compositor") == 0)
		display->compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
	else if (strcmp(interface, "wl_shell") == 0)
		display->shell = wl_registry_bind(registry, id, &wl_shell_interface, 1);
	else if (strcmp(interface, "wl_shm") == 0)
		display->shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
}

static void wl_registry_handle_global_remove(void* data, struct wl_registry* registry, uint32_t name)
{

}

static const struct wl_registry_listener wl_registry_listener =
{
	wl_registry_handle_global,
	wl_registry_handle_global_remove
};

int wl_context_new(freerdp* instance, rdpContext* context)
{
	context->channels = freerdp_channels_new();

	return 0;
}

void wl_context_free(freerdp* instance, rdpContext* context)
{

}

void wl_begin_paint(rdpContext* context)
{
	rdpGdi* gdi;

	gdi = context->gdi;
	gdi->primary->hdc->hwnd->invalid->null = 1;
}

void wl_end_paint(rdpContext* context)
{
	rdpGdi* gdi;
	struct display* display;
	struct window* window;
	struct wl_context* context_w;

	gdi = context->gdi;
	if (gdi->primary->hdc->hwnd->invalid->null)
		return;

	context_w = (struct wl_context*) context;
	display = context_w->display;
	window = context_w->window;

	realloc(window->data, gdi->width * gdi->height * 4);
	memcpy(window->data, (void*) gdi->primary_buffer, gdi->width * gdi->height * 4);
	wl_display_dispatch(display->display);
}

BOOL wl_pre_connect(freerdp* instance)
{
	struct display* display;
	struct wl_context* context;

	freerdp_channels_pre_connect(instance->context->channels, instance);

	display = malloc(sizeof(*display));
	display->display = wl_display_connect(NULL);

	if (!display->display)
	{
		WLog_ERR(TAG, "wl_pre_connect: failed to connect to Wayland compositor");
		WLog_ERR(TAG, "Please check that the XDG_RUNTIME_DIR environment variable is properly set.");
		free(display);
		return FALSE;
	}

	display->registry = wl_display_get_registry(display->display);
	wl_registry_add_listener(display->registry, &wl_registry_listener, display);
	wl_display_roundtrip(display->display);

	if (!display->compositor || !display->shell || !display->shm)
	{
		WLog_ERR(TAG, "wl_pre_connect: failed to find needed compositor interfaces");
		free(display);
		return FALSE;
	}

	 /* put Wayland data in the context here */
	context = (struct wl_context*) instance->context;
	context->display = display;

	return TRUE;
}

BOOL wl_post_connect(freerdp* instance)
{
	struct window* window;
	struct wl_context* context;

	context = (struct wl_context*) instance->context;

	window = malloc(sizeof(*window));
	window->width = instance->settings->DesktopWidth;
	window->height = instance->settings->DesktopHeight;
	window->buffers[0].busy = 0;
	window->buffers[1].busy = 0;
	window->callback = NULL;
	window->display = context->display;
	window->surface = wl_compositor_create_surface(window->display->compositor);
	window->shell_surface = wl_shell_get_shell_surface(window->display->shell, window->surface);

	wl_shell_surface_add_listener(window->shell_surface, &wl_shell_surface_listener, NULL);
	wl_shell_surface_set_title(window->shell_surface, "FreeRDP");
	wl_shell_surface_set_toplevel(window->shell_surface);
	wl_surface_damage(window->surface, 0, 0, window->width, window->height);

	 /* GC/GDI logic here */
	rdpGdi* gdi;

	gdi_init(instance, CLRCONV_ALPHA | CLRCONV_INVERT | CLRBUF_32BPP, NULL);
	gdi = instance->context->gdi;

	 /* fill buffer with first image here */
	window->data = malloc (gdi->width * gdi->height *4);
	memcpy(window->data, (void*) gdi->primary_buffer, gdi->width * gdi->height * 4);
	instance->update->BeginPaint = wl_begin_paint;
	instance->update->EndPaint = wl_end_paint;

	 /* put Wayland data in the context here */
	context->window = window;

	freerdp_channels_post_connect(instance->context->channels, instance);

	window_redraw(window, NULL, 0);

	return TRUE;
}

BOOL wl_verify_certificate(freerdp* instance, char* subject, char* issuer, char* fingerprint)
{
	char answer;

	printf("Certificate details:\n");
	printf("\tSubject: %s\n", subject);
	printf("\tIssuer: %s\n", issuer);
	printf("\tThumbprint: %s\n", fingerprint);
	printf("The above X.509 certificate could not be verified, possibly because you do not have "
		"the CA certificate in your certificate store, or the certificate has expired. "
		"Please look at the documentation on how to create local certificate store for a private CA.\n");

	while (1)
	{
		printf("Do you trust the above certificate? (Y/N) ");
		answer = fgetc(stdin);

		if (feof(stdin))
		{
			printf("\nError: Could not read answer from stdin.");
			if (instance->settings->CredentialsFromStdin)
				printf(" - Run without parameter \"--from-stdin\" to set trust.");
			printf("\n");
			return FALSE;
		}

		if (answer == 'y' || answer == 'Y')
		{
			return TRUE;
		}
		else if (answer == 'n' || answer == 'N')
		{
			break;
		}
		printf("\n");
	}

	return FALSE;
}

int wlfreerdp_run(freerdp* instance)
{
	int i;
	int fds;
	int max_fds;
	int rcount;
	int wcount;
	void* rfds[32];
	void* wfds[32];
	fd_set rfds_set;
	fd_set wfds_set;

	ZeroMemory(rfds, sizeof(rfds));
	ZeroMemory(wfds, sizeof(wfds));

	freerdp_connect(instance);

	while (1)
	{
		rcount = 0;
		wcount = 0;
		if (freerdp_get_fds(instance, rfds, &rcount, wfds, &wcount) != TRUE)
		{
			printf("Failed to get FreeRDP file descriptor");
			break;
		}
		if (freerdp_channels_get_fds(instance->context->channels, instance, rfds, &rcount, wfds, &wcount) != TRUE)
		{
			printf("Failed to get FreeRDP file descriptor");
			break;
		}

		max_fds = 0;
		FD_ZERO(&rfds_set);
		FD_ZERO(&wfds_set);

		for (i = 0; i < rcount; i++)
		{
			fds = (int)(long)(rfds[i]);

			if (fds > max_fds)
				max_fds = fds;

			FD_SET(fds, &rfds_set);
		}

		if (max_fds == 0)
			break;

		if (select(max_fds + 1, &rfds_set, &wfds_set, NULL, NULL) == -1)
		{
			if (!((errno == EAGAIN) ||
				(errno == EWOULDBLOCK) ||
				(errno == EINPROGRESS) ||
				(errno == EINTR)))
			{
				printf("wlfreerdp_run: select failed\n");
				break;
			}
		}

		if (freerdp_check_fds(instance) != TRUE)
		{
			printf("Failed to check FreeRDP file descriptor\n");
			break;
		}
		if (freerdp_channels_check_fds(instance->context->channels, instance) != TRUE)
		{
			printf("Failed to check channel manager file descriptor\n");
			break;
		}
	}

	struct display* display;
	struct window* window;
	struct wl_context* context;

	context = (struct wl_context*) instance->context;
	display = context->display;
	window = context->window;
	free(window->buffers[0].shm_data);
	free(window->buffers[1].shm_data);
	free(window->data);

	wl_buffer_destroy(window->buffers[0].buffer);
	wl_buffer_destroy(window->buffers[1].buffer);
	wl_shell_surface_destroy(window->shell_surface);
	wl_surface_destroy(window->surface);
	wl_shm_destroy(display->shm);
	wl_shell_destroy(display->shell);
	wl_compositor_destroy(display->compositor);
	wl_registry_destroy(display->registry);
	wl_display_disconnect(display->display);

	freerdp_channels_close(instance->context->channels, instance);
	freerdp_channels_free(instance->context->channels);
	freerdp_free(instance);

	return 0;
}

int main(int argc, char* argv[])
{
	int status;
	freerdp* instance;

	instance = freerdp_new();
	instance->PreConnect = wl_pre_connect;
	instance->PostConnect = wl_post_connect;
	instance->VerifyCertificate = wl_verify_certificate;

	instance->ContextSize = sizeof(struct wl_context);
	instance->ContextNew = wl_context_new;
	instance->ContextFree = wl_context_free;
	freerdp_context_new(instance);

	status = freerdp_client_settings_parse_command_line_arguments(instance->settings, argc, argv);

	status = freerdp_client_settings_command_line_status_print(instance->settings, status, argc, argv);

	if (status)
		exit(0);

	freerdp_client_load_addins(instance->context->channels, instance->settings);

	wlfreerdp_run(instance);

	return 0;
}
