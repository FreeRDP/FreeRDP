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

#include "rdtk_font.h"

#include "rdtk_text_field.h"

int rdtk_text_field_draw(rdtkSurface* surface, int nXDst, int nYDst, int nWidth, int nHeight,
                         rdtkTextField* textField, const char* text)
{
	int offsetX;
	int offsetY;
	int textWidth;
	int textHeight;
	int fillWidth;
	int fillHeight;
	rdtkFont* font;
	rdtkEngine* engine;
	rdtkNinePatch* ninePatch;

	engine = surface->engine;
	font = engine->font;
	textField = surface->engine->textField;
	ninePatch = textField->ninePatch;

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

rdtkTextField* rdtk_text_field_new(rdtkEngine* engine, rdtkNinePatch* ninePatch)
{
	rdtkTextField* textField;

	textField = (rdtkTextField*)calloc(1, sizeof(rdtkTextField));

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
	if (!engine->textField)
	{
		engine->textField = rdtk_text_field_new(engine, engine->textField9patch);
	}

	return 1;
}

int rdtk_text_field_engine_uninit(rdtkEngine* engine)
{
	if (engine->textField)
	{
		rdtk_text_field_free(engine->textField);
		engine->textField = NULL;
	}

	return 1;
}
