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

#ifndef RDTK_FONT_PRIVATE_H
#define RDTK_FONT_PRIVATE_H

#include <stdint.h>

#include <rdtk/rdtk.h>

#include <winpr/image.h>

#include "rdtk_engine.h"

struct rdtk_glyph
{
	int width;
	int offsetX;
	int offsetY;
	int rectX;
	int rectY;
	int rectWidth;
	int rectHeight;
	uint8_t code[4];
};

struct rdtk_font
{
	rdtkEngine* engine;

	uint32_t size;
	uint16_t height;
	char* family;
	char* style;
	wImage* image;
	uint16_t glyphCount;
	rdtkGlyph* glyphs;
};

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_ATTR_NODISCARD
	int rdtk_font_text_draw_size(rdtkFont* font, uint16_t* width, uint16_t* height,
	                             const char* text);

	WINPR_ATTR_NODISCARD
	int rdtk_font_engine_init(rdtkEngine* engine);

	int rdtk_font_engine_uninit(rdtkEngine* engine);

	void rdtk_font_free(rdtkFont* font);

	WINPR_ATTR_MALLOC(rdtk_font_free, 1)
	WINPR_ATTR_NODISCARD
	rdtkFont* rdtk_font_new(rdtkEngine* engine, const char* path, const char* file);

#ifdef __cplusplus
}
#endif

#endif /* RDTK_FONT_PRIVATE_H */
