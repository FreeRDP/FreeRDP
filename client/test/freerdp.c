/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP Test UI
 *
 * Copyright 2010 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "gdi.h"

#include <freerdp/freerdp.h>
#include <freerdp/utils/args.h>

freerdp* instance;
rdpSettings* settings;

int main(int argc, char* argv[])
{
	instance = freerdp_new();

	settings = instance->settings;

	freerdp_parse_args(settings, argc, argv, NULL, NULL, NULL, NULL);

	gdi_init(instance, 0);

	instance->Connect(instance);

	freerdp_free(instance);

	return 0;
}
