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

#ifndef RDTK_H
#define RDTK_H

#include <rdtk/api.h>

#include <winpr/crt.h>
#include <winpr/windows.h>

#include <freerdp/codec/color.h>
#include <freerdp/codec/region.h>

typedef struct rdtk_engine rdtkEngine;
typedef struct rdtk_font rdtkFont;
typedef struct rdtk_glyph rdtkGlyph;
typedef struct rdtk_surface rdtkSurface;
typedef struct rdtk_button rdtkButton;
typedef struct rdtk_label rdtkLabel;
typedef struct rdtk_text_field rdtkTextField;
typedef struct rdtk_nine_patch rdtkNinePatch;

#ifdef __cplusplus
extern "C"
{
#endif

	/* Engine */

	RDTK_EXPORT rdtkEngine* rdtk_engine_new();
	RDTK_EXPORT void rdtk_engine_free(rdtkEngine* engine);

	/* Surface */

	RDTK_EXPORT int rdtk_surface_fill(rdtkSurface* surface, int x, int y, int width, int height,
	                                  UINT32 color);

	RDTK_EXPORT rdtkSurface* rdtk_surface_new(rdtkEngine* engine, BYTE* data, int width, int height,
	                                          int scanline);
	RDTK_EXPORT void rdtk_surface_free(rdtkSurface* surface);

	/* Font */

	RDTK_EXPORT int rdtk_font_draw_text(rdtkSurface* surface, int nXDst, int nYDst, rdtkFont* font,
	                                    const char* text);

	/* Button */

	RDTK_EXPORT int rdtk_button_draw(rdtkSurface* surface, int nXDst, int nYDst, int nWidth,
	                                 int nHeight, rdtkButton* button, const char* text);

	/* Label */

	RDTK_EXPORT int rdtk_label_draw(rdtkSurface* surface, int nXDst, int nYDst, int nWidth,
	                                int nHeight, rdtkLabel* label, const char* text, int hAlign,
	                                int vAlign);

	/* TextField */

	RDTK_EXPORT int rdtk_text_field_draw(rdtkSurface* surface, int nXDst, int nYDst, int nWidth,
	                                     int nHeight, rdtkTextField* textField, const char* text);

#ifdef __cplusplus
}
#endif

#endif /* RDTK_H */
