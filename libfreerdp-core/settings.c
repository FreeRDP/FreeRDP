/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RDP Settings
 *
 * Copyright 2009-2011 Jay Sorg
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

#include <freerdp/utils/memory.h>

#include <freerdp/settings.h>

rdpSettings* settings_new()
{
	rdpSettings* settings;

	settings = (rdpSettings*) xzalloc(sizeof(rdpSettings));

	if (settings != NULL)
	{
		/* assign default settings */

		settings->width = 1024;
		settings->height = 768;
		settings->color_depth = 16;
		settings->nla_security = 1;
		settings->tls_security = 1;
		settings->rdp_security = 1;
		settings->encryption = 1;
	}

	return settings;
}

void settings_free(rdpSettings* settings)
{
	if (settings != NULL)
	{
		xfree(settings);
	}
}
