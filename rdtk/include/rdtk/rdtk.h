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

typedef struct rdtk_font rdtkFont;
typedef struct rdtk_glyph rdtkGlyph;
typedef struct rdtk_surface rdtkSurface;

#ifdef __cplusplus
extern "C" {
#endif

/* Surface */

RDTK_EXPORT rdtkSurface* rdtk_surface_new(BYTE* data, int width, int height, int scanline);
RDTK_EXPORT void rdtk_surface_free(rdtkSurface* surface);

/* Font */

RDTK_EXPORT int rdtk_font_draw_text(rdtkSurface* surface, int nXDst, int nYDst, rdtkFont* font, const char* text);

RDTK_EXPORT rdtkFont* rdtk_font_new(const char* path, const char* file);
RDTK_EXPORT void rdtk_font_free(rdtkFont* font);

#ifdef __cplusplus
}
#endif

#endif /* RDTK_H */

