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

#include <freerdp/config.h>

#include <winpr/assert.h>

#include "shadow_surface.h"

#include "shadow_screen.h"
#include "shadow_lobby.h"

rdpShadowScreen* shadow_screen_new(rdpShadowServer* server)
{
	INT64 x, y;
	INT64 width, height;
	rdpShadowScreen* screen;
	rdpShadowSubsystem* subsystem;
	MONITOR_DEF* primary;

	WINPR_ASSERT(server);
	WINPR_ASSERT(server->subsystem);

	screen = (rdpShadowScreen*)calloc(1, sizeof(rdpShadowScreen));

	if (!screen)
		goto out_error;

	screen->server = server;
	subsystem = server->subsystem;

	if (!InitializeCriticalSectionAndSpinCount(&(screen->lock), 4000))
		goto out_free;

	region16_init(&(screen->invalidRegion));

	WINPR_ASSERT(subsystem->selectedMonitor < ARRAYSIZE(subsystem->monitors));
	primary = &(subsystem->monitors[subsystem->selectedMonitor]);

	x = primary->left;
	y = primary->top;
	width = primary->right - primary->left + 1;
	height = primary->bottom - primary->top + 1;

	WINPR_ASSERT(x >= 0);
	WINPR_ASSERT(x <= UINT16_MAX);
	WINPR_ASSERT(y >= 0);
	WINPR_ASSERT(y <= UINT16_MAX);
	WINPR_ASSERT(width >= 0);
	WINPR_ASSERT(width <= UINT16_MAX);
	WINPR_ASSERT(height >= 0);
	WINPR_ASSERT(height <= UINT16_MAX);

	screen->width = (UINT16)width;
	screen->height = (UINT16)height;

	screen->primary =
	    shadow_surface_new(server, (UINT16)x, (UINT16)y, (UINT16)width, (UINT16)height);

	if (!screen->primary)
		goto out_free_region;

	server->surface = screen->primary;

	screen->lobby = shadow_surface_new(server, (UINT16)x, (UINT16)y, (UINT16)width, (UINT16)height);

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
	width = primary->right - primary->left + 1;
	height = primary->bottom - primary->top + 1;

	WINPR_ASSERT(x >= 0);
	WINPR_ASSERT(x <= UINT16_MAX);
	WINPR_ASSERT(y >= 0);
	WINPR_ASSERT(y <= UINT16_MAX);
	WINPR_ASSERT(width >= 0);
	WINPR_ASSERT(width <= UINT16_MAX);
	WINPR_ASSERT(height >= 0);
	WINPR_ASSERT(height <= UINT16_MAX);
	if (shadow_surface_resize(screen->primary, (UINT16)x, (UINT16)y, (UINT16)width,
	                          (UINT16)height) &&
	    shadow_surface_resize(screen->lobby, (UINT16)x, (UINT16)y, (UINT16)width, (UINT16)height))
	{
		if (((UINT32)width != screen->width) || ((UINT32)height != screen->height))
		{
			/* screen size is changed. Store new size and reinit lobby */
			screen->width = (UINT32)width;
			screen->height = (UINT32)height;
			shadow_client_init_lobby(screen->server);
		}
		return TRUE;
	}

	return FALSE;
}
