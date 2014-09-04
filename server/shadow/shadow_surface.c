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

#include "shadow.h"

#include "shadow_surface.h"

rdpShadowSurface* shadow_surface_new(rdpShadowServer* server, int x, int y, int width, int height)
{
	rdpShadowSurface* surface;

	surface = (rdpShadowSurface*) calloc(1, sizeof(rdpShadowSurface));

	if (!surface)
		return NULL;

	surface->server = server;

	surface->x = x;
	surface->y = y;
	surface->width = width;
	surface->height = height;
	surface->scanline = (surface->width + (surface->width % 4)) * 4;

	surface->data = (BYTE*) malloc(surface->scanline * surface->height);

	if (!surface->data)
		return NULL;

	ZeroMemory(surface->data, surface->scanline * surface->height);

	if (!InitializeCriticalSectionAndSpinCount(&(surface->lock), 4000))
		return NULL;

	region16_init(&(surface->invalidRegion));

	return surface;
}

void shadow_surface_free(rdpShadowSurface* surface)
{
	if (!surface)
		return;

	free(surface->data);

	DeleteCriticalSection(&(surface->lock));

	region16_uninit(&(surface->invalidRegion));

	free(surface);
}
