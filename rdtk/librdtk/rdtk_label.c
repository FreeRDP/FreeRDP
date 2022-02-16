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

#include "rdtk_font.h"

#include "rdtk_label.h"

int rdtk_label_draw(rdtkSurface* surface, uint16_t nXDst, uint16_t nYDst, uint16_t nWidth,
                    uint16_t nHeight, rdtkLabel* label, const char* text, uint16_t hAlign,
                    uint16_t vAlign)
{
	uint16_t offsetX;
	uint16_t offsetY;
	uint16_t textWidth;
	uint16_t textHeight;
	rdtkFont* font;
	rdtkEngine* engine;

	engine = surface->engine;
	font = engine->font;

	rdtk_font_text_draw_size(font, &textWidth, &textHeight, text);

	if ((textWidth > 0) && (textHeight > 0))
	{
		offsetX = 0;
		offsetY = 0;

		if (textWidth < nWidth)
			offsetX = ((nWidth - textWidth) / 2);

		if (textHeight < nHeight)
			offsetY = ((nHeight - textHeight) / 2);

		rdtk_font_draw_text(surface, nXDst + offsetX, nYDst + offsetY, font, text);
	}

	return 1;
}

rdtkLabel* rdtk_label_new(rdtkEngine* engine)
{
	rdtkLabel* label;

	label = (rdtkLabel*)calloc(1, sizeof(rdtkLabel));

	if (!label)
		return NULL;

	label->engine = engine;

	return label;
}

void rdtk_label_free(rdtkLabel* label)
{
	free(label);
}

int rdtk_label_engine_init(rdtkEngine* engine)
{
	if (!engine->label)
	{
		engine->label = rdtk_label_new(engine);
	}

	return 1;
}

int rdtk_label_engine_uninit(rdtkEngine* engine)
{
	if (engine->label)
	{
		rdtk_label_free(engine->label);
		engine->label = NULL;
	}

	return 1;
}
