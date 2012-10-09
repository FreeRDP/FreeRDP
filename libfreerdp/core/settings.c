/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "certificate.h"
#include "capabilities.h"

#include <freerdp/utils/memory.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <winpr/crt.h>
#include <winpr/registry.h>

#include <freerdp/settings.h>
#include <freerdp/utils/file.h>

static const char client_dll[] = "C:\\Windows\\System32\\mstscax.dll";

void settings_client_load_hkey_local_machine(rdpSettings* settings)
{
	HKEY hKey;
	LONG status;
	DWORD dwType;
	DWORD dwSize;
	DWORD dwValue;

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\FreeRDP\\Client"), 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status != ERROR_SUCCESS)
		return;

	dwSize = sizeof(DWORD);
	if (RegQueryValueEx(hKey, _T("DesktopWidth"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
		settings->width = dwValue;

	dwSize = sizeof(DWORD);
	if (RegQueryValueEx(hKey, _T("DesktopHeight"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
		settings->height = dwValue;

	dwSize = sizeof(DWORD);
	if (RegQueryValueEx(hKey, _T("KeyboardType"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
		settings->kbd_type = dwValue;

	dwSize = sizeof(DWORD);
	if (RegQueryValueEx(hKey, _T("KeyboardSubType"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
		settings->kbd_subtype = dwValue;

	dwSize = sizeof(DWORD);
	if (RegQueryValueEx(hKey, _T("KeyboardFunctionKeys"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
		settings->kbd_fn_keys = dwValue;

	dwSize = sizeof(DWORD);
	if (RegQueryValueEx(hKey, _T("KeyboardLayout"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
		settings->kbd_layout = dwValue;

	dwSize = sizeof(DWORD);
	if (RegQueryValueEx(hKey, _T("NlaSecurity"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
		settings->nla_security = dwValue ? 1 : 0;

	dwSize = sizeof(DWORD);
	if (RegQueryValueEx(hKey, _T("TlsSecurity"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
		settings->tls_security = dwValue ? 1 : 0;

	dwSize = sizeof(DWORD);
	if (RegQueryValueEx(hKey, _T("RdpSecurity"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
		settings->rdp_security = dwValue ? 1 : 0;

	RegCloseKey(hKey);
}

void settings_server_load_hkey_local_machine(rdpSettings* settings)
{
	HKEY hKey;
	LONG status;
	DWORD dwType;
	DWORD dwSize;
	DWORD dwValue;

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\FreeRDP\\Server"), 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status != ERROR_SUCCESS)
		return;

	if (RegQueryValueEx(hKey, _T("NlaSecurity"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
		settings->nla_security = dwValue ? 1 : 0;

	if (RegQueryValueEx(hKey, _T("TlsSecurity"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
		settings->tls_security = dwValue ? 1 : 0;

	if (RegQueryValueEx(hKey, _T("RdpSecurity"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
		settings->rdp_security = dwValue ? 1 : 0;

	RegCloseKey(hKey);
}

void settings_load_hkey_local_machine(rdpSettings* settings)
{
	if (settings->server_mode)
		settings_server_load_hkey_local_machine(settings);
	else
		settings_client_load_hkey_local_machine(settings);
}

rdpSettings* settings_new(void* instance)
{
	rdpSettings* settings;

	settings = (rdpSettings*) xzalloc(sizeof(rdpSettings));

	if (settings != NULL)
	{
		settings->instance = instance;

		/* Server instances are NULL */

		if (!settings->instance)
			settings->server_mode = TRUE;

		settings->width = 1024;
		settings->height = 768;
		settings->workarea = FALSE;
		settings->fullscreen = FALSE;
		settings->grab_keyboard = TRUE;
		settings->decorations = TRUE;
		settings->rdp_version = 7;
		settings->color_depth = 16;
		settings->nla_security = TRUE;
		settings->tls_security = TRUE;
		settings->rdp_security = TRUE;
		settings->security_layer_negotiation = TRUE;
		settings->client_build = 2600;
		settings->kbd_type = 4; /* @msdn{cc240510} 'IBM enhanced (101- or 102-key) keyboard' */
		settings->kbd_subtype = 0;
		settings->kbd_fn_keys = 12;
		settings->kbd_layout = 0;
		settings->encryption = FALSE;
		settings->salted_checksum = TRUE;
		settings->port = 3389;
		settings->desktop_resize = TRUE;

		settings->performance_flags =
				PERF_DISABLE_FULLWINDOWDRAG |
				PERF_DISABLE_MENUANIMATIONS |
				PERF_DISABLE_WALLPAPER;

		settings->auto_reconnection = TRUE;

		settings->encryption_method = ENCRYPTION_METHOD_NONE;
		settings->encryption_level = ENCRYPTION_LEVEL_NONE;

		settings->authentication = TRUE;
		settings->authentication_only = FALSE;
		settings->from_stdin = FALSE;

		settings->received_caps = xzalloc(32);
		settings->order_support = xzalloc(32);

		settings->order_support[NEG_DSTBLT_INDEX] = TRUE;
		settings->order_support[NEG_PATBLT_INDEX] = TRUE;
		settings->order_support[NEG_SCRBLT_INDEX] = TRUE;
		settings->order_support[NEG_OPAQUE_RECT_INDEX] = TRUE;
		settings->order_support[NEG_DRAWNINEGRID_INDEX] = TRUE;
		settings->order_support[NEG_MULTIDSTBLT_INDEX] = TRUE;
		settings->order_support[NEG_MULTIPATBLT_INDEX] = TRUE;
		settings->order_support[NEG_MULTISCRBLT_INDEX] = TRUE;
		settings->order_support[NEG_MULTIOPAQUERECT_INDEX] = TRUE;
		settings->order_support[NEG_MULTI_DRAWNINEGRID_INDEX] = TRUE;
		settings->order_support[NEG_LINETO_INDEX] = TRUE;
		settings->order_support[NEG_POLYLINE_INDEX] = TRUE;
		settings->order_support[NEG_MEMBLT_INDEX] = TRUE;
		settings->order_support[NEG_MEM3BLT_INDEX] = TRUE;
		settings->order_support[NEG_SAVEBITMAP_INDEX] = TRUE;
		settings->order_support[NEG_GLYPH_INDEX_INDEX] = TRUE;
		settings->order_support[NEG_FAST_INDEX_INDEX] = TRUE;
		settings->order_support[NEG_FAST_GLYPH_INDEX] = TRUE;
		settings->order_support[NEG_POLYGON_SC_INDEX] = TRUE;
		settings->order_support[NEG_POLYGON_CB_INDEX] = TRUE;
		settings->order_support[NEG_ELLIPSE_SC_INDEX] = TRUE;
		settings->order_support[NEG_ELLIPSE_CB_INDEX] = TRUE;

		settings->client_hostname = xzalloc(32);
		settings->client_product_id = xzalloc(32);

		settings->color_pointer = TRUE;
		settings->large_pointer = TRUE;
		settings->pointer_cache_size = 20;
		settings->sound_beeps = TRUE;
		settings->disable_wallpaper = FALSE;
		settings->disable_full_window_drag = FALSE;
		settings->disable_menu_animations = FALSE;
		settings->disable_theming = FALSE;
		settings->connection_type = 0;

		settings->draw_gdi_plus = FALSE;

		settings->frame_marker = FALSE;
		settings->bitmap_cache_v3 = FALSE;

		settings->bitmap_cache = TRUE;
		settings->persistent_bitmap_cache = FALSE;
		settings->bitmapCacheV2CellInfo = xzalloc(sizeof(BITMAP_CACHE_V2_CELL_INFO) * 6);

		settings->refresh_rect = TRUE;
		settings->suppress_output = TRUE;

		settings->glyph_cache = TRUE;
		settings->glyphSupportLevel = GLYPH_SUPPORT_NONE;
		settings->glyphCache = xzalloc(sizeof(GLYPH_CACHE_DEFINITION) * 10);
		settings->fragCache = xnew(GLYPH_CACHE_DEFINITION);
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
		settings->glyphCache[9].cacheMaximumCellSize = 256;
		settings->fragCache->cacheEntries = 256;
		settings->fragCache->cacheMaximumCellSize = 256;

		settings->offscreen_bitmap_cache = TRUE;
		settings->offscreen_bitmap_cache_size = 7680;
		settings->offscreen_bitmap_cache_entries = 2000;

		settings->draw_nine_grid_cache_size = 2560;
		settings->draw_nine_grid_cache_entries = 256;

		settings->client_dir = _strdup(client_dll);

		settings->num_icon_caches = 3;
		settings->num_icon_cache_entries = 12;

		settings->vc_chunk_size = CHANNEL_CHUNK_LENGTH;

		settings->multifrag_max_request_size = 0x200000;

		settings->fastpath_input = TRUE;
		settings->fastpath_output = TRUE;

		settings->frame_acknowledge = 2;

		gethostname(settings->client_hostname, 31);
		settings->client_hostname[31] = 0;
		settings->mouse_motion = TRUE;

		settings->client_auto_reconnect_cookie = xnew(ARC_CS_PRIVATE_PACKET);
		settings->server_auto_reconnect_cookie = xnew(ARC_SC_PRIVATE_PACKET);

		settings->client_time_zone = xnew(TIME_ZONE_INFO);

		freerdp_detect_paths(settings);

		settings_load_hkey_local_machine(settings);
	}

	return settings;
}

void settings_free(rdpSettings* settings)
{
	if (settings != NULL)
	{
		free(settings->hostname);
		free(settings->username);
		free(settings->password);
		free(settings->domain);
		free(settings->shell);
		free(settings->directory);
		free(settings->ip_address);
		free(settings->client_dir);
		free(settings->cert_file);
		free(settings->privatekey_file);
		free(settings->received_caps);
		free(settings->order_support);
		free(settings->client_hostname);
		free(settings->client_product_id);
		free(settings->server_random);
		free(settings->server_certificate);
		free(settings->rdp_key_file);
		certificate_free(settings->server_cert);
		free(settings->client_auto_reconnect_cookie);
		free(settings->server_auto_reconnect_cookie);
		free(settings->client_time_zone);
		free(settings->bitmapCacheV2CellInfo);
		free(settings->glyphCache);
		free(settings->fragCache);
		key_free(settings->server_key);
		free(settings->config_path);
		free(settings->current_path);
		free(settings->development_path);
		free(settings);
	}
}
