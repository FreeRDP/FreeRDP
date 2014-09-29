/**
 * RdTk: Remote Desktop Toolkit
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

#include "rdtk_surface.h"

rdtkSurface* rdtk_surface_new(BYTE* data, int width, int height, int scanline)
{
	rdtkSurface* surface;

	surface = (rdtkSurface*) calloc(1, sizeof(rdtkSurface));

	if (!surface)
		return NULL;

	surface->width = width;
	surface->height = height;

	if (scanline < 0)
		scanline = width * 4;

	surface->scanline = scanline;

	surface->data = data;
	surface->owner = FALSE;

	if (!data)
	{
		surface->scanline = (surface->width + (surface->width % 4)) * 4;

		surface->data = (BYTE*) malloc(surface->scanline * surface->height);

		if (!surface->data)
			return NULL;

		ZeroMemory(surface->data, surface->scanline * surface->height);

		surface->owner = TRUE;
	}

	return surface;
}

void rdtk_surface_free(rdtkSurface* surface)
{
	if (!surface)
		return;

	if (surface->owner)
		free(surface->data);

	free(surface);
}

