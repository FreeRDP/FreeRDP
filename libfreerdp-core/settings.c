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

#include "config.h"
#include "capabilities.h"
#include <freerdp/utils/memory.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

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
		settings->workarea = False;
		settings->fullscreen = False;
		settings->decorations = True;
		settings->rdp_version = 7;
		settings->color_depth = 16;
		settings->nla_security = True;
		settings->tls_security = True;
		settings->rdp_security = True;
		settings->client_build = 2600;
		settings->kbd_type = 0;
		settings->kbd_subtype = 0;
		settings->kbd_fn_keys = 0;
		settings->kbd_layout = 0x409;
		settings->encryption = False;
		settings->port = 3389;

		settings->performance_flags =
				PERF_DISABLE_FULLWINDOWDRAG |
				PERF_DISABLE_MENUANIMATIONS |
				PERF_DISABLE_WALLPAPER;

		settings->auto_reconnection = True;

		settings->encryption_method = ENCRYPTION_METHOD_NONE;
		settings->encryption_level = ENCRYPTION_LEVEL_NONE;

		settings->authentication = True;

		settings->order_support[NEG_DSTBLT_INDEX] = True;
		settings->order_support[NEG_PATBLT_INDEX] = True;
		settings->order_support[NEG_SCRBLT_INDEX] = True;
		settings->order_support[NEG_OPAQUE_RECT_INDEX] = True;
		settings->order_support[NEG_DRAWNINEGRID_INDEX] = True;
		settings->order_support[NEG_MULTIDSTBLT_INDEX] = True;
		settings->order_support[NEG_MULTIPATBLT_INDEX] = True;
		settings->order_support[NEG_MULTISCRBLT_INDEX] = True;
		settings->order_support[NEG_MULTIOPAQUERECT_INDEX] = True;
		settings->order_support[NEG_MULTI_DRAWNINEGRID_INDEX] = True;
		settings->order_support[NEG_LINETO_INDEX] = True;
		settings->order_support[NEG_POLYLINE_INDEX] = True;
		settings->order_support[NEG_MEMBLT_INDEX] = True;
		settings->order_support[NEG_MEM3BLT_INDEX] = True;
		settings->order_support[NEG_SAVEBITMAP_INDEX] = True;
		settings->order_support[NEG_GLYPH_INDEX_INDEX] = True;
		settings->order_support[NEG_FAST_INDEX_INDEX] = True;
		settings->order_support[NEG_FAST_GLYPH_INDEX] = True;
		settings->order_support[NEG_POLYGON_SC_INDEX] = True;
		settings->order_support[NEG_POLYGON_CB_INDEX] = True;
		settings->order_support[NEG_ELLIPSE_SC_INDEX] = True;
		settings->order_support[NEG_ELLIPSE_CB_INDEX] = True;

		settings->color_pointer = True;
		settings->large_pointer = True;
		settings->pointer_cache_size = 32;

		settings->draw_gdi_plus = False;

		settings->frame_marker = False;
		settings->bitmap_cache_v3 = False;

		settings->bitmap_cache = True;
		settings->persistent_bitmap_cache = False;

		settings->glyphSupportLevel = GLYPH_SUPPORT_NONE;
		settings->glyphCache[0].cacheEntries = 254;
		settings->glyphCache[0].cacheMaximumCellSize = 4;
		settings->glyphCache[1].cacheEntries = 254;
		settings->glyphCache[1].cacheMaximumCellSize = 4;
		settings->glyphCache[2].cacheEntries = 254;
		settings->glyphCache[2].cacheMaximumCellSize = 8;
		settings->glyphCache[3].cacheEntries = 254;
		settings->glyphCache[3].cacheMaximumCellSize = 8;
		settings->glyphCache[4].cacheEntries = 254;
		settings->glyphCache[4].cacheMaximumCellSize = 16;
		settings->glyphCache[5].cacheEntries = 254;
		settings->glyphCache[5].cacheMaximumCellSize = 32;
		settings->glyphCache[6].cacheEntries = 254;
		settings->glyphCache[6].cacheMaximumCellSize = 64;
		settings->glyphCache[7].cacheEntries = 254;
		settings->glyphCache[7].cacheMaximumCellSize = 128;
		settings->glyphCache[8].cacheEntries = 254;
		settings->glyphCache[8].cacheMaximumCellSize = 256;
		settings->glyphCache[9].cacheEntries = 64;
		settings->glyphCache[9].cacheMaximumCellSize = 248;
		settings->fragCache.cacheEntries = 64;
		settings->fragCache.cacheMaximumCellSize = 248;

		settings->offscreen_bitmap_cache = True;
		settings->offscreen_bitmap_cache_size = 7680;
		settings->offscreen_bitmap_cache_entries = 100;

		settings->draw_nine_grid_cache_size = 2560;
		settings->draw_nine_grid_cache_entries = 256;

		settings->client_dir = xstrdup(client_dll);

		settings->num_icon_caches = 3;
		settings->num_icon_cache_entries = 12;

		settings->vc_chunk_size = CHANNEL_CHUNK_LENGTH;

		settings->multifrag_max_request_size = 0x200000;

		settings->fastpath_input = True;
		settings->fastpath_output = True;

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
		xfree(settings->hostname);
		xfree(settings->username);
		xfree(settings->password);
		xfree(settings->domain);
		xfree(settings->shell);
		xfree(settings->directory);
		xfree(settings->ip_address);
		xfree(settings->client_dir);
		xfree(settings->cert_file);
		xfree(settings->privatekey_file);
		xfree(settings);
	}
}
