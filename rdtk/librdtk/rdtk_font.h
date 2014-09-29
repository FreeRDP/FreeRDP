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

#include <rdtk/rdtk.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/image.h>

struct rdtk_glyph
{
	int width;
	int offsetX;
	int offsetY;
	int rectX;
	int rectY;
	int rectWidth;
	int rectHeight;
	BYTE code[4];
};

struct rdtk_font
{
	int size;
	int height;
	char* family;
	char* style;
	wImage* image;
	int glyphCount;
	rdtkGlyph* glyphs;
};

#ifdef __cplusplus
extern "C" {
#endif

int rdtk_load_embedded_fonts();

#ifdef __cplusplus
}
#endif

#endif /* RDTK_FONT_PRIVATE_H */

