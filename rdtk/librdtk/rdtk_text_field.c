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

#include "rdtk_text_field.h"

int rdtk_text_field_draw(rdtkSurface* surface, uint16_t nXDst, uint16_t nYDst, uint16_t nWidth,
                         uint16_t nHeight, rdtkTextField* textField, const char* text)
{
	uint16_t textWidth = 0;
	uint16_t textHeight = 0;

	WINPR_ASSERT(surface);
	WINPR_ASSERT(textField);
	WINPR_ASSERT(text);

	rdtkEngine* engine = surface->engine;
	rdtkFont* font = engine->font;
	textField = surface->engine->textField;
	rdtkNinePatch* ninePatch = textField->ninePatch;

	rdtk_font_text_draw_size(font, &textWidth, &textHeight, text);

	rdtk_nine_patch_draw(surface, nXDst, nYDst, nWidth, nHeight, ninePatch);

	if ((textWidth > 0) && (textHeight > 0))
	{
		const int fwd = (ninePatch->width - ninePatch->fillWidth);
		const int fhd = (ninePatch->height - ninePatch->fillHeight);

		uint16_t fillWidth = nWidth - WINPR_ASSERTING_INT_CAST(uint16_t, fwd);
		uint16_t fillHeight = nHeight - WINPR_ASSERTING_INT_CAST(uint16_t, fhd);
		uint16_t offsetX = WINPR_ASSERTING_INT_CAST(uint16_t, ninePatch->fillLeft);
		uint16_t offsetY = WINPR_ASSERTING_INT_CAST(uint16_t, ninePatch->fillTop);

		if (textWidth < fillWidth)
		{
			const int wd = ((fillWidth - textWidth) / 2) + ninePatch->fillLeft;
			offsetX = WINPR_ASSERTING_INT_CAST(uint16_t, wd);
		}
		else if (textWidth < ninePatch->width)
		{
			const int wd = ((ninePatch->width - textWidth) / 2);
			offsetX = WINPR_ASSERTING_INT_CAST(uint16_t, wd);
		}

		if (textHeight < fillHeight)
		{
			const int wd = ((fillHeight - textHeight) / 2) + ninePatch->fillTop;
			offsetY = WINPR_ASSERTING_INT_CAST(uint16_t, wd);
		}
		else if (textHeight < ninePatch->height)
		{
			const int wd = ((ninePatch->height - textHeight) / 2);
			offsetY = WINPR_ASSERTING_INT_CAST(uint16_t, wd);
		}

		rdtk_font_draw_text(surface, nXDst + offsetX, nYDst + offsetY, font, text);
	}

	return 1;
}

rdtkTextField* rdtk_text_field_new(rdtkEngine* engine, rdtkNinePatch* ninePatch)
{
	WINPR_ASSERT(engine);
	WINPR_ASSERT(ninePatch);

	rdtkTextField* textField = (rdtkTextField*)calloc(1, sizeof(rdtkTextField));

	if (!textField)
		return NULL;

	textField->engine = engine;
	textField->ninePatch = ninePatch;

	return textField;
}

void rdtk_text_field_free(rdtkTextField* textField)
{
	free(textField);
}

int rdtk_text_field_engine_init(rdtkEngine* engine)
{
	WINPR_ASSERT(engine);

	if (!engine->textField)
	{
		engine->textField = rdtk_text_field_new(engine, engine->textField9patch);
		if (!engine->textField)
			return -1;
	}

	return 1;
}

int rdtk_text_field_engine_uninit(rdtkEngine* engine)
{
	WINPR_ASSERT(engine);
	if (engine->textField)
	{
		rdtk_text_field_free(engine->textField);
		engine->textField = NULL;
	}

	return 1;
}
