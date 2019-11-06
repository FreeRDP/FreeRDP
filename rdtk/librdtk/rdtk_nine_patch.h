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

#ifndef RDTK_NINE_PATCH_PRIVATE_H
#define RDTK_NINE_PATCH_PRIVATE_H

#include <rdtk/rdtk.h>

#include <winpr/image.h>

#include "rdtk_surface.h"

#include "rdtk_engine.h"

struct rdtk_nine_patch
{
	rdtkEngine* engine;

	wImage* image;

	int width;
	int height;
	int scanline;
	BYTE* data;

	int scaleLeft;
	int scaleRight;
	int scaleWidth;
	int scaleTop;
	int scaleBottom;
	int scaleHeight;

	int fillLeft;
	int fillRight;
	int fillWidth;
	int fillTop;
	int fillBottom;
	int fillHeight;
};

#ifdef __cplusplus
extern "C"
{
#endif

	int rdtk_nine_patch_set_image(rdtkNinePatch* ninePatch, wImage* image);
	int rdtk_nine_patch_draw(rdtkSurface* surface, int nXDst, int nYDst, int nWidth, int nHeight,
	                         rdtkNinePatch* ninePatch);

	int rdtk_nine_patch_engine_init(rdtkEngine* engine);
	int rdtk_nine_patch_engine_uninit(rdtkEngine* engine);

	rdtkNinePatch* rdtk_nine_patch_new(rdtkEngine* engine);
	void rdtk_nine_patch_free(rdtkNinePatch* ninePatch);

#ifdef __cplusplus
}
#endif

#endif /* RDTK_NINE_PATCH_PRIVATE_H */
