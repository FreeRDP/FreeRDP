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

#include <rdtk/rdtk.h>

#include "shadow.h"

#include "shadow_lobby.h"

BOOL shadow_client_init_lobby(rdpShadowServer* server)
{
	int width;
	int height;
	rdtkEngine* engine;
	rdtkSurface* surface;
	RECTANGLE_16 invalidRect;
	rdpShadowSurface* lobby = server->lobby;

	if (!lobby)
		return FALSE;

	if (!(engine = rdtk_engine_new()))
	{
		return FALSE;
	}

	if (!(surface =
	          rdtk_surface_new(engine, lobby->data, lobby->width, lobby->height, lobby->scanline)))
	{
		rdtk_engine_free(engine);
		return FALSE;
	}

	invalidRect.left = 0;
	invalidRect.top = 0;
	invalidRect.right = lobby->width;
	invalidRect.bottom = lobby->height;
	if (server->shareSubRect)
	{
		/* If we have shared sub rect setting, only fill shared rect */
		rectangles_intersection(&invalidRect, &(server->subRect), &invalidRect);
	}

	width = invalidRect.right - invalidRect.left;
	height = invalidRect.bottom - invalidRect.top;
	rdtk_surface_fill(surface, invalidRect.left, invalidRect.top, width, height, 0x3BB9FF);

	rdtk_label_draw(surface, invalidRect.left, invalidRect.top, width, height, NULL, "Welcome", 0,
	                0);
	// rdtk_button_draw(surface, 16, 64, 128, 32, NULL, "button");
	// rdtk_text_field_draw(surface, 16, 128, 128, 32, NULL, "text field");

	rdtk_surface_free(surface);

	rdtk_engine_free(engine);

	region16_union_rect(&(lobby->invalidRegion), &(lobby->invalidRegion), &invalidRect);

	return TRUE;
}
