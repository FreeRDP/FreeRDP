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

#ifndef FREERDP_SHADOW_SERVER_FONT_H
#define FREERDP_SHADOW_SERVER_FONT_H

#include <freerdp/server/shadow.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/image.h>

struct rdp_shadow_glyph
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
typedef struct rdp_shadow_glyph rdpShadowGlyph;

struct rdp_shadow_font
{
	int size;
	int height;
	char* family;
	char* style;
	wImage* image;
	int glyphCount;
	rdpShadowGlyph* glyphs;
};

#ifdef __cplusplus
extern "C" {
#endif

int shadow_font_draw_text(rdpShadowSurface* surface, int nXDst, int nYDst, rdpShadowFont* font, const char* text);
int shadow_font_draw_glyph(rdpShadowSurface* surface, int nXDst, int nYDst, rdpShadowFont* font, rdpShadowGlyph* glyph);

rdpShadowFont* shadow_font_new(const char* path, const char* file);
void shadow_font_free(rdpShadowFont* font);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SHADOW_SERVER_FONT_H */
