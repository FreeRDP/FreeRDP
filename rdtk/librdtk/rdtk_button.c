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
#include <winpr/cast.h>

#include <rdtk/config.h>

#include "rdtk_font.h"

#include "rdtk_button.h"

int rdtk_button_draw(rdtkSurface* surface, uint16_t nXDst, uint16_t nYDst, uint16_t nWidth,
                     uint16_t nHeight, rdtkButton* button, const char* text)
{
	uint16_t textWidth = 0;
	uint16_t textHeight = 0;

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
		const int wd = (ninePatch->width - ninePatch->fillWidth);
		const int hd = (ninePatch->height - ninePatch->fillHeight);

		const uint16_t fillWidth = nWidth - WINPR_ASSERTING_INT_CAST(uint16_t, wd);
		const uint16_t fillHeight = nHeight - WINPR_ASSERTING_INT_CAST(uint16_t, hd);
		uint16_t offsetX = WINPR_ASSERTING_INT_CAST(UINT16, ninePatch->fillLeft);
		uint16_t offsetY = WINPR_ASSERTING_INT_CAST(UINT16, ninePatch->fillTop);

		if (textWidth < fillWidth)
		{
			const int twd = ((fillWidth - textWidth) / 2) + ninePatch->fillLeft;
			offsetX = WINPR_ASSERTING_INT_CAST(uint16_t, twd);
		}
		else if (textWidth < ninePatch->width)
		{
			const int twd = ((ninePatch->width - textWidth) / 2);
			offsetX = WINPR_ASSERTING_INT_CAST(uint16_t, twd);
		}

		if (textHeight < fillHeight)
		{
			const int twd = ((fillHeight - textHeight) / 2) + ninePatch->fillTop;
			offsetY = WINPR_ASSERTING_INT_CAST(uint16_t, twd);
		}
		else if (textHeight < ninePatch->height)
		{
			const int twd = ((ninePatch->height - textHeight) / 2);
			offsetY = WINPR_ASSERTING_INT_CAST(uint16_t, twd);
		}

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
