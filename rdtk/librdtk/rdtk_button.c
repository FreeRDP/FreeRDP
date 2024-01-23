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

#include <winpr/assert.h>

#include <rdtk/config.h>

#include "rdtk_font.h"

#include "rdtk_button.h"

int rdtk_button_draw(rdtkSurface* surface, uint16_t nXDst, uint16_t nYDst, uint16_t nWidth,
                     uint16_t nHeight, rdtkButton* button, const char* text)
{
	uint16_t offsetX = 0;
	uint16_t offsetY = 0;
	uint16_t textWidth = 0;
	uint16_t textHeight = 0;
	uint16_t fillWidth = 0;
	uint16_t fillHeight = 0;

	WINPR_ASSERT(surface);
	WINPR_ASSERT(button);
	WINPR_ASSERT(text);

	rdtkEngine* engine = surface->engine;
	rdtkFont* font = engine->font;
	rdtkNinePatch* ninePatch = button->ninePatch;

	rdtk_font_text_draw_size(font, &textWidth, &textHeight, text);

	rdtk_nine_patch_draw(surface, nXDst, nYDst, nWidth, nHeight, ninePatch);

	if ((textWidth > 0) && (textHeight > 0))
	{
		fillWidth = nWidth - (ninePatch->width - ninePatch->fillWidth);
		fillHeight = nHeight - (ninePatch->height - ninePatch->fillHeight);

		offsetX = ninePatch->fillLeft;
		offsetY = ninePatch->fillTop;

		if (textWidth < fillWidth)
			offsetX = ((fillWidth - textWidth) / 2) + ninePatch->fillLeft;
		else if (textWidth < ninePatch->width)
			offsetX = ((ninePatch->width - textWidth) / 2);

		if (textHeight < fillHeight)
			offsetY = ((fillHeight - textHeight) / 2) + ninePatch->fillTop;
		else if (textHeight < ninePatch->height)
			offsetY = ((ninePatch->height - textHeight) / 2);

		rdtk_font_draw_text(surface, nXDst + offsetX, nYDst + offsetY, font, text);
	}

	return 1;
}

rdtkButton* rdtk_button_new(rdtkEngine* engine, rdtkNinePatch* ninePatch)
{
	WINPR_ASSERT(engine);
	WINPR_ASSERT(ninePatch);

	rdtkButton* button = (rdtkButton*)calloc(1, sizeof(rdtkButton));

	if (!button)
		return NULL;

	button->engine = engine;
	button->ninePatch = ninePatch;

	return button;
}

void rdtk_button_free(rdtkButton* button)
{
	free(button);
}

int rdtk_button_engine_init(rdtkEngine* engine)
{
	WINPR_ASSERT(engine);

	if (!engine->button)
	{
		engine->button = rdtk_button_new(engine, engine->button9patch);
		if (!engine->button)
			return -1;
	}

	return 1;
}

int rdtk_button_engine_uninit(rdtkEngine* engine)
{
	WINPR_ASSERT(engine);

	if (engine->button)
	{
		rdtk_button_free(engine->button);
		engine->button = NULL;
	}

	return 1;
}
