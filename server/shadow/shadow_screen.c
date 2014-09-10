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

rdpShadowScreen* shadow_screen_new(rdpShadowServer* server)
{
	int x, y;
	int width, height;
	MONITOR_DEF* primary;
	rdpShadowScreen* screen;
	rdpShadowSubsystem* subsystem;

	screen = (rdpShadowScreen*) calloc(1, sizeof(rdpShadowScreen));

	if (!screen)
		return NULL;

	screen->server = server;
	subsystem = server->subsystem;

	if (!InitializeCriticalSectionAndSpinCount(&(screen->lock), 4000))
		return NULL;

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
		return NULL;

	server->surface = screen->primary;

	return screen;
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

	free(screen);
}

