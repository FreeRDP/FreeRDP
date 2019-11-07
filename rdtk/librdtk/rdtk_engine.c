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
#include "rdtk_nine_patch.h"
#include "rdtk_button.h"
#include "rdtk_text_field.h"

#include "rdtk_engine.h"

rdtkEngine* rdtk_engine_new()
{
	rdtkEngine* engine;

	engine = (rdtkEngine*)calloc(1, sizeof(rdtkEngine));

	if (!engine)
		return NULL;

	rdtk_font_engine_init(engine);
	rdtk_nine_patch_engine_init(engine);
	rdtk_button_engine_init(engine);
	rdtk_text_field_engine_init(engine);

	return engine;
}

void rdtk_engine_free(rdtkEngine* engine)
{
	if (!engine)
		return;

	rdtk_font_engine_uninit(engine);
	rdtk_nine_patch_engine_uninit(engine);
	rdtk_button_engine_uninit(engine);
	rdtk_text_field_engine_uninit(engine);

	free(engine);
}
