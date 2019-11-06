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

#ifndef RDTK_BUTTON_PRIVATE_H
#define RDTK_BUTTON_PRIVATE_H

#include <rdtk/rdtk.h>

#include "rdtk_surface.h"
#include "rdtk_nine_patch.h"

#include "rdtk_engine.h"

struct rdtk_button
{
	rdtkEngine* engine;
	rdtkNinePatch* ninePatch;
};

#ifdef __cplusplus
extern "C"
{
#endif

	int rdtk_button_engine_init(rdtkEngine* engine);
	int rdtk_button_engine_uninit(rdtkEngine* engine);

	rdtkButton* rdtk_button_new(rdtkEngine* engine, rdtkNinePatch* ninePatch);
	void rdtk_button_free(rdtkButton* button);

#ifdef __cplusplus
}
#endif

#endif /* RDTK_BUTTON_PRIVATE_H */
