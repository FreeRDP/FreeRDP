/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Wayland Displays
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

#include "wlf_display.h"

static void wl_registry_handle_global(void* data, struct wl_registry* registry, uint32_t id, const char *interface, uint32_t version)
{
	wlfDisplay* display = data;

	if (strcmp(interface, "wl_compositor") == 0)
		display->compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
	else if (strcmp(interface, "wl_shell") == 0)
		display->shell = wl_registry_bind(registry, id, &wl_shell_interface, 1);
	else if (strcmp(interface, "wl_shm") == 0)
		display->shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
	else if (strcmp(interface, "wl_seat") == 0)
		display->seat = wl_registry_bind(registry, id, &wl_seat_interface, 1);
}

static void wl_registry_handle_global_remove(void* data, struct wl_registry* registry, uint32_t name)
{

}

static const struct wl_registry_listener wl_registry_listener =
{
	wl_registry_handle_global,
	wl_registry_handle_global_remove
};


wlfDisplay* wlf_CreateDisplay(void)
{
	wlfDisplay* display;

	display = (wlfDisplay*) calloc(1, sizeof(wlfDisplay));

	if (display)
	{
		display->display = wl_display_connect(NULL);

		if (!display->display)
		{
			WLog_ERR(TAG, "wl_pre_connect: failed to connect to Wayland compositor");
			WLog_ERR(TAG, "Please check that the XDG_RUNTIME_DIR environment variable is properly set.");
			free(display);
			return NULL;
		}

		display->registry = wl_display_get_registry(display->display);
		wl_registry_add_listener(display->registry, &wl_registry_listener, display);
		wl_display_roundtrip(display->display);

		if (!display->compositor || !display->shell || !display->shm)
		{
			WLog_ERR(TAG, "wl_pre_connect: failed to find needed compositor interfaces");
			free(display);
			return NULL;
		}
	}

	return display;
}

BOOL wlf_RefreshDisplay(wlfDisplay* display)
{
	if (wl_display_dispatch(display->display) == -1)
		return FALSE;
	return TRUE;
}

void wlf_DestroyDisplay(wlfContext* wlfc, wlfDisplay* display)
{
	if (display == NULL)
		return;

	if (wlfc->display == display)
		wlfc->display = NULL;

	if (display->seat)
		wl_seat_destroy(display->seat);
	if (display->shm)
		wl_shm_destroy(display->shm);
	if (display->shell)
		wl_shell_destroy(display->shell);
	if (display->compositor)
		wl_compositor_destroy(display->compositor);
	if (display->registry)
		wl_registry_destroy(display->registry);
	wl_display_disconnect(display->display);

	free(display);
}
