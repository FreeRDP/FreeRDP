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

#include "rdtk_button.h"

int rdtk_button_draw(rdtkSurface* surface, int nXDst, int nYDst, int nWidth, int nHeight,
		rdtkButton* button, const char* text)
{
	button = surface->engine->button;

	rdtk_nine_patch_draw(surface, nXDst, nYDst, nWidth, nHeight, button->ninePatch);

	return 1;
}

rdtkButton* rdtk_button_new(rdtkEngine* engine, rdtkNinePatch* ninePatch)
{
	rdtkButton* button;

	button = (rdtkButton*) calloc(1, sizeof(rdtkButton));

	if (!button)
		return NULL;

	button->engine = engine;
	button->ninePatch = ninePatch;

	return button;
}

void rdtk_button_free(rdtkButton* button)
{
	if (!button)
		return;

	free(button);
}

int rdtk_button_engine_init(rdtkEngine* engine)
{
	if (!engine->button)
	{
		engine->button = rdtk_button_new(engine, engine->button9patch);
	}

	return 1;
}
