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

#include "capabilities.h"
#include <freerdp/utils/memory.h>

#include <freerdp/settings.h>

static char client_dll[] = "C:\\Windows\\System32\\mstscax.dll";

rdpSettings* settings_new()
{
	rdpSettings* settings;

	settings = (rdpSettings*) xzalloc(sizeof(rdpSettings));

	if (settings != NULL)
	{
		settings->width = 1024;
		settings->height = 768;
		settings->rdp_version = 7;
		settings->color_depth = 16;
		settings->nla_security = 1;
		settings->tls_security = 1;
		settings->rdp_security = 1;
		settings->client_build = 2600;
		settings->kbd_type = 0;
		settings->kbd_subtype = 0;
		settings->kbd_fn_keys = 0;
		settings->kbd_layout = 0x409;
		settings->encryption = False;

		settings->performance_flags =
				PERF_DISABLE_FULLWINDOWDRAG |
				PERF_DISABLE_MENUANIMATIONS |
				PERF_DISABLE_WALLPAPER;

		settings->encryption_method = ENCRYPTION_METHOD_NONE;
		settings->encryption_level = ENCRYPTION_LEVEL_NONE;

		settings->order_support[NEG_DSTBLT_INDEX] = True;
		settings->order_support[NEG_PATBLT_INDEX] = True;
		settings->order_support[NEG_SCRBLT_INDEX] = True;
		settings->order_support[NEG_MEMBLT_INDEX] = True;
		settings->order_support[NEG_MEM3BLT_INDEX] = True;
		settings->order_support[NEG_DRAWNINEGRID_INDEX] = True;
		settings->order_support[NEG_LINETO_INDEX] = True;
		settings->order_support[NEG_MULTI_DRAWNINEGRID_INDEX] = True;
		settings->order_support[NEG_SAVEBITMAP_INDEX] = True;
		settings->order_support[NEG_MULTIDSTBLT_INDEX] = True;
		settings->order_support[NEG_MULTIPATBLT_INDEX] = True;
		settings->order_support[NEG_MULTISCRBLT_INDEX] = True;
		settings->order_support[NEG_MULTIOPAQUERECT_INDEX] = True;
		settings->order_support[NEG_FAST_INDEX_INDEX] = True;
		settings->order_support[NEG_POLYGON_SC_INDEX] = True;
		settings->order_support[NEG_POLYGON_CB_INDEX] = True;
		settings->order_support[NEG_POLYLINE_INDEX] = True;
		settings->order_support[NEG_FAST_GLYPH_INDEX] = True;
		settings->order_support[NEG_ELLIPSE_SC_INDEX] = True;
		settings->order_support[NEG_ELLIPSE_CB_INDEX] = True;
		settings->order_support[NEG_INDEX_INDEX] = True;

		settings->color_pointer = True;

		settings->client_dir = xmalloc(strlen(client_dll));
		strcpy(settings->client_dir, client_dll);

		settings->uniconv = freerdp_uniconv_new();
		gethostname(settings->client_hostname, sizeof(settings->client_hostname) - 1);
	}

	return settings;
}

void settings_free(rdpSettings* settings)
{
	if (settings != NULL)
	{
		freerdp_uniconv_free(settings->uniconv);
		xfree(settings);
	}
}
