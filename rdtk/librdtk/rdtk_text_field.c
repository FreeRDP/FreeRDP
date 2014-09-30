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

#include "rdtk_text_field.h"

int rdtk_text_field_draw(rdtkSurface* surface, int nXDst, int nYDst, int nWidth, int nHeight,
		rdtkTextField* textField, const char* text)
{
	textField = surface->engine->textField;

	rdtk_nine_patch_draw(surface, nXDst, nYDst, nWidth, nHeight, textField->ninePatch);

	return 1;
}

rdtkTextField* rdtk_text_field_new(rdtkEngine* engine, rdtkNinePatch* ninePatch)
{
	rdtkTextField* textField;

	textField = (rdtkTextField*) calloc(1, sizeof(rdtkTextField));

	if (!textField)
		return NULL;

	textField->engine = engine;
	textField->ninePatch = ninePatch;

	return textField;
}

void rdtk_text_field_free(rdtkTextField* textField)
{
	if (!textField)
		return;

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
