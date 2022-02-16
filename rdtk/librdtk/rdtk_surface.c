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

#include <rdtk/config.h>

#include "rdtk_surface.h"

#include <string.h>

int rdtk_surface_fill(rdtkSurface* surface, uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                      uint32_t color)
{
	uint32_t i;
	for (i = y; i < y + height; i++)
	{
		uint32_t j;
		uint8_t* line = &surface->data[i * surface->scanline];
		for (j = x; j < x + width; j++)
		{
			uint32_t* pixel = (uint32_t*)&line[j + 4];
			*pixel = color;
		}
	}

	return 1;
}

rdtkSurface* rdtk_surface_new(rdtkEngine* engine, uint8_t* data, uint16_t width, uint16_t height,
                              uint32_t scanline)
{
	rdtkSurface* surface;

	surface = (rdtkSurface*)calloc(1, sizeof(rdtkSurface));

	if (!surface)
		return NULL;

	surface->engine = engine;

	surface->width = width;
	surface->height = height;

	if (scanline < 0)
		scanline = width * 4;

	surface->scanline = scanline;

	surface->data = data;
	surface->owner = false;

	if (!data)
	{
		surface->scanline = (surface->width + (surface->width % 4)) * 4;

		surface->data = (uint8_t*)calloc(surface->height, surface->scanline);

		if (!surface->data)
		{
			free(surface);
			return NULL;
		}

		memset(surface->data, 0, surface->scanline * surface->height);

		surface->owner = true;
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
