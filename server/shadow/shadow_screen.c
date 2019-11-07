/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "shadow_surface.h"

#include "shadow_screen.h"
#include "shadow_lobby.h"

rdpShadowScreen* shadow_screen_new(rdpShadowServer* server)
{
	int x, y;
	int width, height;
	rdpShadowScreen* screen;
	rdpShadowSubsystem* subsystem;
	MONITOR_DEF* primary;

	screen = (rdpShadowScreen*)calloc(1, sizeof(rdpShadowScreen));

	if (!screen)
		goto out_error;

	screen->server = server;
	subsystem = server->subsystem;

	if (!InitializeCriticalSectionAndSpinCount(&(screen->lock), 4000))
		goto out_free;

	region16_init(&(screen->invalidRegion));

	primary = &(subsystem->monitors[subsystem->selectedMonitor]);

	x = primary->left;
	y = primary->top;
	width = primary->right - primary->left;
	height = primary->bottom - primary->top;

	screen->width = width;
	screen->height = height;

	screen->primary = shadow_surface_new(server, x, y, width, height);

	if (!screen->primary)
		goto out_free_region;

	server->surface = screen->primary;

	screen->lobby = shadow_surface_new(server, x, y, width, height);

	if (!screen->lobby)
		goto out_free_primary;

	server->lobby = screen->lobby;

	shadow_client_init_lobby(server);

	return screen;

out_free_primary:
	shadow_surface_free(screen->primary);
	server->surface = screen->primary = NULL;
out_free_region:
	region16_uninit(&(screen->invalidRegion));
	DeleteCriticalSection(&(screen->lock));
out_free:
	free(screen);
out_error:
	return NULL;
}

void shadow_screen_free(rdpShadowScreen* screen)
{
	if (!screen)
		return;

	DeleteCriticalSection(&(screen->lock));

	region16_uninit(&(screen->invalidRegion));

	if (screen->primary)
	{
		shadow_surface_free(screen->primary);
		screen->primary = NULL;
	}

	if (screen->lobby)
	{
		shadow_surface_free(screen->lobby);
		screen->lobby = NULL;
	}

	free(screen);
}

BOOL shadow_screen_resize(rdpShadowScreen* screen)
{
	int x, y;
	int width, height;
	MONITOR_DEF* primary;
	rdpShadowSubsystem* subsystem;

	if (!screen)
		return FALSE;

	subsystem = screen->server->subsystem;
	primary = &(subsystem->monitors[subsystem->selectedMonitor]);

	x = primary->left;
	y = primary->top;
	width = primary->right - primary->left;
	height = primary->bottom - primary->top;

	if (shadow_surface_resize(screen->primary, x, y, width, height) &&
	    shadow_surface_resize(screen->lobby, x, y, width, height))
	{
		if ((width != screen->width) || (height != screen->height))
		{
			/* screen size is changed. Store new size and reinit lobby */
			screen->width = width;
			screen->height = height;
			shadow_client_init_lobby(screen->server);
		}
		return TRUE;
	}

	return FALSE;
}
