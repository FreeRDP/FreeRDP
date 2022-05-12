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

#include "shadow.h"

#include "shadow_surface.h"
#define ALIGN_SCREEN_SIZE(size, align) \
	((((size) % (align)) != 0) ? ((size) + (align) - ((size) % (align))) : (size))

rdpShadowSurface* shadow_surface_new(rdpShadowServer* server, UINT16 x, UINT16 y, UINT32 width,
                                     UINT32 height)
{
	rdpShadowSurface* surface;
	surface = (rdpShadowSurface*)calloc(1, sizeof(rdpShadowSurface));

	if (!surface)
		return NULL;

	surface->server = server;
	surface->x = x;
	surface->y = y;
	surface->width = width;
	surface->height = height;
	surface->scanline = ALIGN_SCREEN_SIZE(surface->width, 32) * 4;
	surface->format = PIXEL_FORMAT_BGRX32;
	surface->data = (BYTE*)calloc(ALIGN_SCREEN_SIZE(surface->height, 32), surface->scanline);

	if (!surface->data)
	{
		free(surface);
		return NULL;
	}

	if (!InitializeCriticalSectionAndSpinCount(&(surface->lock), 4000))
	{
		free(surface->data);
		free(surface);
		return NULL;
	}

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

BOOL shadow_surface_resize(rdpShadowSurface* surface, UINT16 x, UINT16 y, UINT32 width,
                           UINT32 height)
{
	BYTE* buffer = NULL;
	UINT32 scanline = ALIGN_SCREEN_SIZE(width, 4) * 4;

	if (!surface)
		return FALSE;

	if ((width == surface->width) && (height == surface->height))
	{
		/* We don't need to reset frame buffer, just update left top */
		surface->x = x;
		surface->y = y;
		return TRUE;
	}

	buffer = (BYTE*)realloc(surface->data, scanline * ALIGN_SCREEN_SIZE(height, 4));

	if (buffer)
	{
		surface->x = x;
		surface->y = y;
		surface->width = width;
		surface->height = height;
		surface->scanline = scanline;
		surface->data = buffer;
		return TRUE;
	}

	return FALSE;
}
