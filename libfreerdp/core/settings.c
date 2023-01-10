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

#include <freerdp/config.h>

#include "certificate.h"
#include "capabilities.h"

#include <ctype.h>

#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/sysinfo.h>
#include <winpr/registry.h>

#include <freerdp/settings.h>
#include <freerdp/build-config.h>
#include <ctype.h>

#include "settings.h"

#define TAG FREERDP_TAG("settings")

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#endif

static const char client_dll[] = "C:\\Windows\\System32\\mstscax.dll";

#define SERVER_KEY "Software\\" FREERDP_VENDOR_STRING "\\" FREERDP_PRODUCT_STRING "\\Server"
#define CLIENT_KEY "Software\\" FREERDP_VENDOR_STRING "\\" FREERDP_PRODUCT_STRING "\\Client"
#define BITMAP_CACHE_KEY CLIENT_KEY "\\BitmapCacheV2"
#define GLYPH_CACHE_KEY CLIENT_KEY "\\GlyphCache"
#define POINTER_CACHE_KEY CLIENT_KEY "\\PointerCache"

static BOOL settings_reg_query_dword_val(HKEY hKey, const TCHAR* sub, DWORD* value)
{
	DWORD dwType;
	DWORD dwSize;

	dwSize = sizeof(DWORD);
	if (RegQueryValueEx(hKey, sub, NULL, &dwType, (BYTE*)value, &dwSize) != ERROR_SUCCESS)
		return FALSE;
	if (dwType != REG_DWORD)
		return FALSE;

	return TRUE;
}

static BOOL settings_reg_query_word_val(HKEY hKey, const TCHAR* sub, UINT16* value)
{
	DWORD dwValue;

	if (!settings_reg_query_dword_val(hKey, sub, &dwValue))
		return FALSE;

	if (dwValue > UINT16_MAX)
		return FALSE;

	*value = (UINT16)dwValue;
	return TRUE;
}

static BOOL settings_reg_query_bool_val(HKEY hKey, const TCHAR* sub, BOOL* value)
{
	DWORD dwValue;

	if (!settings_reg_query_dword_val(hKey, sub, &dwValue))
		return FALSE;
	*value = dwValue != 0;
	return TRUE;
}

static BOOL settings_reg_query_dword(rdpSettings* settings, size_t id, HKEY hKey, const TCHAR* sub)
{
	DWORD dwValue;

	if (!settings_reg_query_dword_val(hKey, sub, &dwValue))
		return FALSE;

	return freerdp_settings_set_uint32(settings, id, dwValue);
}

static BOOL settings_reg_query_bool(rdpSettings* settings, size_t id, HKEY hKey, const TCHAR* sub)
{
	DWORD dwValue;

	if (!settings_reg_query_dword_val(hKey, sub, &dwValue))
		return FALSE;

	return freerdp_settings_set_bool(settings, id, dwValue != 0 ? TRUE : FALSE);
}

static void settings_client_load_hkey_local_machine(rdpSettings* settings)
{
	HKEY hKey;
	LONG status;
	status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, CLIENT_KEY, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		settings_reg_query_dword(settings, FreeRDP_DesktopWidth, hKey, _T("DesktopWidth"));
		settings_reg_query_dword(settings, FreeRDP_DesktopHeight, hKey, _T("DesktopHeight"));
		settings_reg_query_bool(settings, FreeRDP_Fullscreen, hKey, _T("Fullscreen"));
		settings_reg_query_dword(settings, FreeRDP_ColorDepth, hKey, _T("ColorDepth"));
		settings_reg_query_dword(settings, FreeRDP_KeyboardType, hKey, _T("KeyboardType"));
		settings_reg_query_dword(settings, FreeRDP_KeyboardSubType, hKey, _T("KeyboardSubType"));
		settings_reg_query_dword(settings, FreeRDP_KeyboardFunctionKey, hKey,
		                         _T("KeyboardFunctionKeys"));
		settings_reg_query_dword(settings, FreeRDP_KeyboardLayout, hKey, _T("KeyboardLayout"));
		settings_reg_query_bool(settings, FreeRDP_ExtSecurity, hKey, _T("ExtSecurity"));
		settings_reg_query_bool(settings, FreeRDP_NlaSecurity, hKey, _T("NlaSecurity"));
		settings_reg_query_bool(settings, FreeRDP_TlsSecurity, hKey, _T("TlsSecurity"));
		settings_reg_query_bool(settings, FreeRDP_RdpSecurity, hKey, _T("RdpSecurity"));
		settings_reg_query_bool(settings, FreeRDP_MstscCookieMode, hKey, _T("MstscCookieMode"));
		settings_reg_query_dword(settings, FreeRDP_CookieMaxLength, hKey, _T("CookieMaxLength"));
		settings_reg_query_bool(settings, FreeRDP_BitmapCacheEnabled, hKey, _T("BitmapCache"));
		settings_reg_query_bool(settings, FreeRDP_OffscreenSupportLevel, hKey,
		                        _T("OffscreenBitmapCache"));
		settings_reg_query_dword(settings, FreeRDP_OffscreenCacheSize, hKey,
		                         _T("OffscreenBitmapCacheSize"));
		settings_reg_query_dword(settings, FreeRDP_OffscreenCacheEntries, hKey,
		                         _T("OffscreenBitmapCacheEntries"));
		RegCloseKey(hKey);
	}

	status =
	    RegOpenKeyExA(HKEY_LOCAL_MACHINE, BITMAP_CACHE_KEY, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		unsigned x;

		settings_reg_query_dword(settings, FreeRDP_BitmapCacheV2NumCells, hKey, _T("NumCells"));
		for (x = 0; x < 5; x++)
		{
			DWORD val;
			TCHAR numentries[64] = { 0 };
			TCHAR persist[64] = { 0 };
			BITMAP_CACHE_V2_CELL_INFO cache = { 0 };
			_sntprintf(numentries, ARRAYSIZE(numentries), _T("Cell%uNumEntries"), x);
			_sntprintf(persist, ARRAYSIZE(persist), _T("Cell%uPersistent"), x);
			if (!settings_reg_query_dword_val(hKey, numentries, &val) ||
			    !settings_reg_query_bool_val(hKey, persist, &cache.persistent) ||
			    !freerdp_settings_set_pointer_array(settings, FreeRDP_BitmapCacheV2CellInfo, x,
			                                        &cache))
				WLog_WARN(TAG, "Failed to load registry keys to settings!");
			cache.numEntries = val;
		}

		settings_reg_query_bool(settings, FreeRDP_AllowCacheWaitingList, hKey,
		                        _T("AllowCacheWaitingList"));
		RegCloseKey(hKey);
	}

	status =
	    RegOpenKeyExA(HKEY_LOCAL_MACHINE, GLYPH_CACHE_KEY, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		unsigned x;
		UINT32 GlyphSupportLevel = 0;
		settings_reg_query_dword(settings, FreeRDP_GlyphSupportLevel, hKey, _T("SupportLevel"));
		for (x = 0; x < 10; x++)
		{
			GLYPH_CACHE_DEFINITION cache = { 0 };
			TCHAR numentries[64] = { 0 };
			TCHAR maxsize[64] = { 0 };
			_sntprintf(numentries, ARRAYSIZE(numentries), _T("Cache%uNumEntries"), x);
			_sntprintf(maxsize, ARRAYSIZE(maxsize), _T("Cache%uMaxCellSize"), x);

			settings_reg_query_word_val(hKey, numentries, &cache.cacheEntries);
			settings_reg_query_word_val(hKey, maxsize, &cache.cacheMaximumCellSize);
			if (!freerdp_settings_set_pointer_array(settings, FreeRDP_GlyphCache, x, &cache))
				WLog_WARN(TAG, "Failed to store GlyphCache %" PRIuz, x);
		}
		{
			GLYPH_CACHE_DEFINITION cache = { 0 };
			settings_reg_query_word_val(hKey, _T("FragCacheNumEntries"), &cache.cacheEntries);
			settings_reg_query_word_val(hKey, _T("FragCacheMaxCellSize"),
			                            &cache.cacheMaximumCellSize);
			if (!freerdp_settings_set_pointer_array(settings, FreeRDP_FragCache, x, &cache))
				WLog_WARN(TAG, "Failed to store FragCache");
		}

		RegCloseKey(hKey);

		if (!freerdp_settings_set_uint32(settings, FreeRDP_GlyphSupportLevel, GlyphSupportLevel))
			WLog_WARN(TAG, "Failed to load registry keys to settings!");
	}

	status =
	    RegOpenKeyExA(HKEY_LOCAL_MACHINE, POINTER_CACHE_KEY, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		settings_reg_query_dword(settings, FreeRDP_LargePointerFlag, hKey, _T("LargePointer"));
		settings_reg_query_dword(settings, FreeRDP_PointerCacheSize, hKey, _T("PointerCacheSize"));
		settings_reg_query_dword(settings, FreeRDP_ColorPointerCacheSize, hKey,
		                         _T("ColorPointerCacheSize"));
		RegCloseKey(hKey);
	}
}

static void settings_server_load_hkey_local_machine(rdpSettings* settings)
{
	HKEY hKey;
	LONG status;

	status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, SERVER_KEY, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status != ERROR_SUCCESS)
		return;

	settings_reg_query_bool(settings, FreeRDP_ExtSecurity, hKey, _T("ExtSecurity"));
	settings_reg_query_bool(settings, FreeRDP_NlaSecurity, hKey, _T("NlaSecurity"));
	settings_reg_query_bool(settings, FreeRDP_TlsSecLevel, hKey, _T("TlsSecurity"));
	settings_reg_query_bool(settings, FreeRDP_RdpSecurity, hKey, _T("RdpSecurity"));

	RegCloseKey(hKey);
}

static void settings_load_hkey_local_machine(rdpSettings* settings)
{
	if (freerdp_settings_get_bool(settings, FreeRDP_ServerMode))
		settings_server_load_hkey_local_machine(settings);
	else
		settings_client_load_hkey_local_machine(settings);
}

static BOOL settings_get_computer_name(rdpSettings* settings)
{
	CHAR computerName[256] = { 0 };
	DWORD nSize = sizeof(computerName);

	if (!GetComputerNameExA(ComputerNameNetBIOS, computerName, &nSize))
		return FALSE;

	if (nSize > MAX_COMPUTERNAME_LENGTH)
		computerName[MAX_COMPUTERNAME_LENGTH] = '\0';

	return freerdp_settings_set_string(settings, FreeRDP_ComputerName, computerName);
}

BOOL freerdp_settings_set_default_order_support(rdpSettings* settings)
{
	BYTE* OrderSupport = freerdp_settings_get_pointer_writable(settings, FreeRDP_OrderSupport);
	if (!OrderSupport)
		return FALSE;

	ZeroMemory(OrderSupport, 32);
	OrderSupport[NEG_DSTBLT_INDEX] = TRUE;
	OrderSupport[NEG_PATBLT_INDEX] = TRUE;
	OrderSupport[NEG_SCRBLT_INDEX] = TRUE;
	OrderSupport[NEG_OPAQUE_RECT_INDEX] = TRUE;
	OrderSupport[NEG_DRAWNINEGRID_INDEX] = FALSE;
	OrderSupport[NEG_MULTIDSTBLT_INDEX] = FALSE;
	OrderSupport[NEG_MULTIPATBLT_INDEX] = FALSE;
	OrderSupport[NEG_MULTISCRBLT_INDEX] = FALSE;
	OrderSupport[NEG_MULTIOPAQUERECT_INDEX] = TRUE;
	OrderSupport[NEG_MULTI_DRAWNINEGRID_INDEX] = FALSE;
	OrderSupport[NEG_LINETO_INDEX] = TRUE;
	OrderSupport[NEG_POLYLINE_INDEX] = TRUE;
	OrderSupport[NEG_MEMBLT_INDEX] =
	    freerdp_settings_get_bool(settings, FreeRDP_BitmapCacheEnabled) ? 1 : 0;
	OrderSupport[NEG_MEM3BLT_INDEX] =
	    freerdp_settings_get_bool(settings, FreeRDP_BitmapCacheEnabled) ? 1 : 0;
	OrderSupport[NEG_MEMBLT_V2_INDEX] =
	    freerdp_settings_get_bool(settings, FreeRDP_BitmapCacheEnabled) ? 1 : 0;
	OrderSupport[NEG_MEM3BLT_V2_INDEX] =
	    freerdp_settings_get_bool(settings, FreeRDP_BitmapCacheEnabled) ? 1 : 0;
	OrderSupport[NEG_SAVEBITMAP_INDEX] = FALSE;
	OrderSupport[NEG_GLYPH_INDEX_INDEX] =
	    freerdp_settings_get_uint32(settings, FreeRDP_GlyphSupportLevel) != GLYPH_SUPPORT_NONE;
	OrderSupport[NEG_FAST_INDEX_INDEX] =
	    freerdp_settings_get_uint32(settings, FreeRDP_GlyphSupportLevel) != GLYPH_SUPPORT_NONE;
	OrderSupport[NEG_FAST_GLYPH_INDEX] =
	    freerdp_settings_get_uint32(settings, FreeRDP_GlyphSupportLevel) != GLYPH_SUPPORT_NONE;
	OrderSupport[NEG_POLYGON_SC_INDEX] = FALSE;
	OrderSupport[NEG_POLYGON_CB_INDEX] = FALSE;
	OrderSupport[NEG_ELLIPSE_SC_INDEX] = FALSE;
	OrderSupport[NEG_ELLIPSE_CB_INDEX] = FALSE;
	return TRUE;
}

BOOL freerdp_capability_buffer_allocate(rdpSettings* settings, UINT32 count)
{
	void* tmp;
	WINPR_ASSERT(settings);
	WINPR_ASSERT(count > 0);
	WINPR_ASSERT(count == 32);

	freerdp_capability_buffer_free(settings);
	WINPR_ASSERT(settings->ReceivedCapabilitiesSize == 0);

	settings->ReceivedCapabilitiesSize = count;
	tmp = realloc(settings->ReceivedCapabilities, count * sizeof(BYTE));
	if (!tmp)
		return FALSE;
	memset(tmp, 0, count * sizeof(BYTE));
	settings->ReceivedCapabilities = tmp;

	tmp = realloc(settings->ReceivedCapabilityData, count * sizeof(BYTE*));
	if (!tmp)
		return FALSE;
	memset(tmp, 0, count * sizeof(BYTE*));
	settings->ReceivedCapabilityData = tmp;

	tmp = realloc(settings->ReceivedCapabilityDataSizes, count * sizeof(UINT32));
	if (!tmp)
		return FALSE;
	memset(tmp, 0, count * sizeof(UINT32));
	settings->ReceivedCapabilityDataSizes = tmp;

	return (settings->ReceivedCapabilities && settings->ReceivedCapabilityData &&
	        settings->ReceivedCapabilityDataSizes);
}

rdpSettings* freerdp_settings_new(DWORD flags)
{
	size_t x;
	char* base;
	char* issuers[] = { "FreeRDP", "FreeRDP-licenser" };
	rdpSettings* settings = (rdpSettings*)calloc(1, sizeof(rdpSettings));

	if (!settings)
		return NULL;

	if (!freerdp_settings_set_string(settings, FreeRDP_ServerLicenseCompanyName, "FreeRDP"))
		goto out_fail;
	if (!freerdp_settings_set_string(settings, FreeRDP_ServerLicenseProductName,
	                                 "FreeRDP-licensing-server"))
		goto out_fail;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_ServerLicenseProductVersion, 1))
		goto out_fail;
	if (!freerdp_server_license_issuers_copy(settings, issuers, ARRAYSIZE(issuers)))
		goto out_fail;

	if (!freerdp_settings_set_bool(settings, FreeRDP_UnicodeInput, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_HasHorizontalWheel, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_HasExtendedMouseEvent, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_HiDefRemoteApp, TRUE) ||
	    !freerdp_settings_set_uint32(
	        settings, FreeRDP_RemoteApplicationSupportMask,
	        RAIL_LEVEL_SUPPORTED | RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED |
	            RAIL_LEVEL_SHELL_INTEGRATION_SUPPORTED | RAIL_LEVEL_LANGUAGE_IME_SYNC_SUPPORTED |
	            RAIL_LEVEL_SERVER_TO_CLIENT_IME_SYNC_SUPPORTED |
	            RAIL_LEVEL_HIDE_MINIMIZED_APPS_SUPPORTED | RAIL_LEVEL_WINDOW_CLOAKING_SUPPORTED |
	            RAIL_LEVEL_HANDSHAKE_EX_SUPPORTED) ||
	    !freerdp_settings_set_uint16(settings, FreeRDP_TextANSICodePage, CP_UTF8) ||
	    !freerdp_settings_set_uint16(settings, FreeRDP_OrderSupportFlags,
	                                 NEGOTIATE_ORDER_SUPPORT | ZERO_BOUNDS_DELTA_SUPPORT |
	                                     COLOR_INDEX_SUPPORT) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_SupportHeartbeatPdu, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_ServerMode,
	                               (flags & FREERDP_SETTINGS_SERVER_MODE) ? TRUE : FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_WaitForOutputBufferFlush, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_ClusterInfoFlags,
	                                 REDIRECTION_SUPPORTED | (REDIRECTION_VERSION4 << 2)) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, 1024) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, 768) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_Workarea, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_Fullscreen, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GrabKeyboard, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_Decorations, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_RdpVersion, RDP_VERSION_10_11) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 16) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_ExtSecurity, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_NegotiateSecurityLayer, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_RestrictedAdminModeRequired, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_MstscCookieMode, FALSE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_CookieMaxLength,
	                                 DEFAULT_COOKIE_MAX_LENGTH) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_ClientBuild,
	                                 18363) || /* Windows 10, Version 1909 */
	    !freerdp_settings_set_uint32(settings, FreeRDP_KeyboardType, 4) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_KeyboardSubType, 0) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_KeyboardFunctionKey, 12) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_KeyboardLayout, 0) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_KeyboardHook,
	                                 KEYBOARD_HOOK_FULLSCREEN_ONLY) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_UseRdpSecurityLayer, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_SaltedChecksum, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, 3389) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_GatewayPort, 443) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_DesktopResize, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_ToggleFullscreen, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_DesktopPosX, UINT32_MAX) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_DesktopPosY, UINT32_MAX) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_SoftwareGdi, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_UnmapButtons, FALSE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_PerformanceFlags, PERF_FLAG_NONE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_AllowFontSmoothing, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_AllowDesktopComposition, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_DisableWallpaper, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_DisableFullWindowDrag, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_DisableMenuAnims, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_DisableThemes, FALSE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_ConnectionType, CONNECTION_TYPE_LAN) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_NetworkAutoDetect, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_EncryptionMethods, ENCRYPTION_METHOD_NONE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_EncryptionLevel, ENCRYPTION_LEVEL_NONE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_FIPSMode, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_CompressionEnabled, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_LogonNotify, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_BrushSupportLevel, BRUSH_COLOR_FULL) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_CompressionLevel, PACKET_COMPR_TYPE_RDP61) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_Authentication, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_AuthenticationOnly, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_CredentialsFromStdin, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_DisableCredentialsDelegation, FALSE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_AuthenticationLevel, 2) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_ChannelCount, 0) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_CertificateUseKnownHosts, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_CertificateCallbackPreferPEM, FALSE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_KeySpec, AT_KEYEXCHANGE))
		goto out_fail;

	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_ChannelDefArray, NULL,
	                                      CHANNEL_MAX_COUNT))
		goto out_fail;

	if (!freerdp_settings_set_bool(settings, FreeRDP_SupportMonitorLayoutPdu, FALSE))
		goto out_fail;

	if (!freerdp_settings_set_uint32(settings, FreeRDP_MonitorCount, 0))
		goto out_fail;

	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_MonitorDefArray, NULL, 32))
		goto out_fail;

	if (!freerdp_settings_set_uint32(settings, FreeRDP_MonitorLocalShiftX, 0))
		goto out_fail;

	if (!freerdp_settings_set_uint32(settings, FreeRDP_MonitorLocalShiftY, 0))
		goto out_fail;

	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_MonitorIds, NULL, 0))
		goto out_fail;

	if (!settings_get_computer_name(settings))
		goto out_fail;

	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RdpServerCertificate, NULL, 1))
		goto out_fail;

	if (!freerdp_capability_buffer_allocate(settings, 32))
		goto out_fail;

	{
		char tmp[32] = { 0 };
		if (!freerdp_settings_set_string_len(settings, FreeRDP_ClientProductId, tmp, sizeof(tmp)))
			goto out_fail;
	}

	{
		char ClientHostname[33] = { 0 };
		DWORD size = sizeof(ClientHostname) - 2;
		GetComputerNameA(ClientHostname, &size);
		if (!freerdp_settings_set_string(settings, FreeRDP_ClientHostname, ClientHostname))
			goto out_fail;
	}

	/* [MS-RDPBCGR] 2.2.7.1.5 Pointer Capability Set (TS_POINTER_CAPABILITYSET)
	 *
	 * if we are in server mode send a reasonable large cache size,
	 * if we are in client mode just set the value to the maximum we want to
	 * support and during capability exchange that size will be limited to the
	 * sizes the server supports
	 *
	 * We have chosen 128 cursors in cache which is at worst 128 * 576kB (384x384 pixel cursor with
	 * 32bit color depth)
	 * */
	if (freerdp_settings_get_bool(settings, FreeRDP_ServerMode))
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_PointerCacheSize, 25) ||
		    !freerdp_settings_set_uint32(settings, FreeRDP_ColorPointerCacheSize, 25))
			goto out_fail;
	}
	else
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_PointerCacheSize, 128) ||
		    !freerdp_settings_set_uint32(settings, FreeRDP_ColorPointerCacheSize, 128))
			goto out_fail;
	}

	if (!freerdp_settings_set_uint32(settings, FreeRDP_LargePointerFlag,
	                                 (LARGE_POINTER_FLAG_96x96 | LARGE_POINTER_FLAG_384x384)) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_SoundBeepsEnabled, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_DrawGdiPlusEnabled, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_DrawAllowSkipAlpha, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_DrawAllowColorSubsampling, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_DrawAllowDynamicColorFidelity, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_FrameMarkerCommandEnabled, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_SurfaceFrameMarkerEnabled, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_AllowCacheWaitingList, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_BitmapCacheV2NumCells, 5))
		goto out_fail;
	settings->BitmapCacheV2CellInfo =
	    (BITMAP_CACHE_V2_CELL_INFO*)calloc(6, sizeof(BITMAP_CACHE_V2_CELL_INFO));

	if (!settings->BitmapCacheV2CellInfo)
		goto out_fail;

	{
		BITMAP_CACHE_V2_CELL_INFO cache = { 0 };
		cache.numEntries = 600;
		if (!freerdp_settings_set_pointer_array(settings, FreeRDP_BitmapCacheV2CellInfo, 0,
		                                        &cache) ||
		    !freerdp_settings_set_pointer_array(settings, FreeRDP_BitmapCacheV2CellInfo, 1, &cache))
			goto out_fail;
		cache.numEntries = 2048;
		if (!freerdp_settings_set_pointer_array(settings, FreeRDP_BitmapCacheV2CellInfo, 2,
		                                        &cache) ||
		    !freerdp_settings_set_pointer_array(settings, FreeRDP_BitmapCacheV2CellInfo, 4, &cache))
			goto out_fail;
		cache.numEntries = 4096;
		if (!freerdp_settings_set_pointer_array(settings, FreeRDP_BitmapCacheV2CellInfo, 3, &cache))
			goto out_fail;
	}
	if (!freerdp_settings_set_bool(settings, FreeRDP_NoBitmapCompressionHeader, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_RefreshRect, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_SuppressOutput, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_GlyphSupportLevel, GLYPH_SUPPORT_NONE))
		goto out_fail;
	settings->GlyphCache = calloc(10, sizeof(GLYPH_CACHE_DEFINITION));

	if (!settings->GlyphCache)
		goto out_fail;

	settings->FragCache = calloc(1, sizeof(GLYPH_CACHE_DEFINITION));

	if (!settings->FragCache)
		goto out_fail;

	for (x = 0; x < 10; x++)
	{
		GLYPH_CACHE_DEFINITION cache = { 0 };
		cache.cacheEntries = 254;
		switch (x)
		{
			case 0:
			case 1:
				cache.cacheMaximumCellSize = 4;
				break;
			case 2:
			case 3:
				cache.cacheMaximumCellSize = 8;
				break;
			case 4:
				cache.cacheMaximumCellSize = 16;
				break;
			case 5:
				cache.cacheMaximumCellSize = 32;
				break;
			case 6:
				cache.cacheMaximumCellSize = 64;
				break;
			case 7:
				cache.cacheMaximumCellSize = 128;
				break;
			case 8:
				cache.cacheMaximumCellSize = 256;
				break;
			case 9:
				cache.cacheMaximumCellSize = 256;
				break;
		}

		if (!freerdp_settings_set_pointer_array(settings, FreeRDP_GlyphCache, x, &cache))
			goto out_fail;
	}
	{
		GLYPH_CACHE_DEFINITION cache = { 0 };
		cache.cacheEntries = 256;
		cache.cacheMaximumCellSize = 256;
		if (!freerdp_settings_set_pointer_array(settings, FreeRDP_FragCache, 0, &cache))
			goto out_fail;
	}
	if (!freerdp_settings_set_uint32(settings, FreeRDP_OffscreenSupportLevel, 0) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_OffscreenCacheSize, 7680) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_OffscreenCacheEntries, 2000) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_DrawNineGridCacheSize, 2560) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_DrawNineGridCacheEntries, 256) ||
	    !freerdp_settings_set_string(settings, FreeRDP_ClientDir, client_dll) ||
	    !freerdp_settings_get_string(settings, FreeRDP_ClientDir) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_RemoteWndSupportLevel,
	                                 WINDOW_LEVEL_SUPPORTED | WINDOW_LEVEL_SUPPORTED_EX) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_RemoteAppNumIconCaches, 3) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_RemoteAppNumIconCacheEntries, 12) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_VirtualChannelChunkSize,
	                                 CHANNEL_CHUNK_LENGTH) ||
	    /* [MS-RDPBCGR] 2.2.7.2.7 Large Pointer Capability Set (TS_LARGE_POINTER_CAPABILITYSET)
	       requires at least this size */
	    !freerdp_settings_set_uint32(settings, FreeRDP_MultifragMaxRequestSize,
	                                 (flags & FREERDP_SETTINGS_SERVER_MODE) ? 0 : 608299) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GatewayUseSameCredentials, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GatewayBypassLocal, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GatewayRpcTransport, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GatewayUdpTransport, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpUseWebsockets, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpExtAuthSspiNtlm, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_FastPathInput, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_FastPathOutput, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_LongCredentialsSupported, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_FrameAcknowledge, 2) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_MouseMotion, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_NSCodecColorLossLevel, 3) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_NSCodecAllowSubsampling, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_NSCodecAllowDynamicColorFidelity, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_AutoReconnectionEnabled, FALSE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_AutoReconnectMaxRetries, 20) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GfxThinClient, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GfxSmallCache, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GfxProgressive, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GfxProgressiveV2, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GfxPlanar, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GfxH264, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GfxSendQoeAck, FALSE))
		goto out_fail;
	{
		ARC_CS_PRIVATE_PACKET cookie = { 0 };
		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_ClientAutoReconnectCookie, &cookie,
		                                      1))
			goto out_fail;
	}
	{
		ARC_SC_PRIVATE_PACKET cookie = { 0 };
		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_ServerAutoReconnectCookie, &cookie,
		                                      1))
			goto out_fail;
	}

	settings->ClientTimeZone = (LPTIME_ZONE_INFORMATION)calloc(1, sizeof(TIME_ZONE_INFORMATION));

	if (!settings->ClientTimeZone)
		goto out_fail;

	if (!freerdp_settings_set_bool(settings, FreeRDP_TcpKeepAlive, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_TcpKeepAliveRetries, 3) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_TcpKeepAliveDelay, 5) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_TcpKeepAliveInterval, 2) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_TcpAckTimeout, 9000) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_TcpConnectTimeout, 15000))
		goto out_fail;

	if (!freerdp_settings_get_bool(settings, FreeRDP_ServerMode))
	{
		BOOL rc;
		char* path;
		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectClipboard, TRUE))
			goto out_fail;
		/* these values are used only by the client part */
		path = GetKnownPath(KNOWN_PATH_HOME);
		rc = freerdp_settings_set_string(settings, FreeRDP_HomePath, path);
		free(path);

		if (!rc || !freerdp_settings_get_string(settings, FreeRDP_HomePath))
			goto out_fail;

		/* For default FreeRDP continue using same config directory
		 * as in old releases.
		 * Custom builds use <Vendor>/<Product> as config folder. */
		if (_stricmp(FREERDP_VENDOR_STRING, FREERDP_PRODUCT_STRING))
		{
			BOOL res = TRUE;
			base = GetKnownSubPath(KNOWN_PATH_XDG_CONFIG_HOME, FREERDP_VENDOR_STRING);

			if (base)
			{
				char* combined = GetCombinedPath(base, FREERDP_PRODUCT_STRING);
				res = freerdp_settings_set_string(settings, FreeRDP_ConfigPath, combined);
				free(combined);
			}

			free(base);
			if (!res)
				goto out_fail;
		}
		else
		{
			BOOL res;
			size_t i;
			char* cpath;
			char product[sizeof(FREERDP_PRODUCT_STRING)] = { 0 };

			for (i = 0; i < sizeof(product); i++)
				product[i] = tolower(FREERDP_PRODUCT_STRING[i]);

			cpath = GetKnownSubPath(KNOWN_PATH_XDG_CONFIG_HOME, product);
			res = freerdp_settings_set_string(settings, FreeRDP_ConfigPath, cpath);
			free(cpath);
			if (!res)
				goto out_fail;
		}

		if (!freerdp_settings_get_string(settings, FreeRDP_ConfigPath))
			goto out_fail;
	}

	settings_load_hkey_local_machine(settings);

	settings->XSelectionAtom = NULL;
	if (!freerdp_settings_set_string(settings, FreeRDP_ActionScript, "~/.config/freerdp/action.sh"))
		goto out_fail;
	if (!freerdp_settings_set_bool(settings, FreeRDP_SmartcardLogon, FALSE))
		goto out_fail;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_TlsSecLevel, 1))
		goto out_fail;
	settings->OrderSupport = calloc(1, 32);

	freerdp_settings_set_uint16(settings, FreeRDP_TLSMinVersion, TLS1_VERSION);
	freerdp_settings_set_uint16(settings, FreeRDP_TLSMaxVersion, 0);

	if (!settings->OrderSupport)
		goto out_fail;

	if (!freerdp_settings_set_default_order_support(settings))
		goto out_fail;

	{
		BOOL enable = freerdp_settings_get_bool(settings, FreeRDP_ServerMode);

		if (!freerdp_settings_set_bool(settings, FreeRDP_SupportDynamicTimeZone, enable))
			goto out_fail;
		if (!freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, enable))
			goto out_fail;
		if (!freerdp_settings_set_bool(settings, FreeRDP_SupportStatusInfoPdu, enable))
			goto out_fail;
		if (!freerdp_settings_set_bool(settings, FreeRDP_SupportErrorInfoPdu, enable))
			goto out_fail;
	}
	return settings;
out_fail:
	freerdp_settings_free(settings);
	return NULL;
}

static void freerdp_settings_free_internal(rdpSettings* settings)
{
	freerdp_server_license_issuers_free(settings);
	freerdp_target_net_addresses_free(settings);
	freerdp_device_collection_free(settings);
	freerdp_static_channel_collection_free(settings);
	freerdp_dynamic_channel_collection_free(settings);

	freerdp_capability_buffer_free(settings);
	/* Extensions */
	free(settings->XSelectionAtom);
	settings->XSelectionAtom = NULL;

	/* Free all strings, set other pointers NULL */
	freerdp_settings_free_keys(settings, TRUE);
}

void freerdp_settings_free(rdpSettings* settings)
{
	if (!settings)
		return;

	freerdp_settings_free_internal(settings);
	free(settings);
}

static BOOL freerdp_settings_int_buffer_copy(rdpSettings* _settings, const rdpSettings* settings)
{
	BOOL rc = FALSE;
	UINT32 index;

	if (!_settings || !settings)
		return FALSE;

	{
		const void* data = freerdp_settings_get_pointer(settings, FreeRDP_LoadBalanceInfo);
		const UINT32 len = freerdp_settings_get_uint32(settings, FreeRDP_LoadBalanceInfoLength);
		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_LoadBalanceInfo, data, len))
			return FALSE;
	}
	{
		const void* data = freerdp_settings_get_pointer(settings, FreeRDP_ServerRandom);
		const UINT32 len = freerdp_settings_get_uint32(settings, FreeRDP_ServerRandomLength);
		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_ServerRandom, data, len))
			return FALSE;
	}
	{
		const void* data = freerdp_settings_get_pointer(settings, FreeRDP_ClientRandom);
		const UINT32 len = freerdp_settings_get_uint32(settings, FreeRDP_ClientRandomLength);
		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_ClientRandom, data, len))
			return FALSE;
	}
	if (!freerdp_server_license_issuers_copy(_settings, settings->ServerLicenseProductIssuers,
	                                         settings->ServerLicenseProductIssuersCount))
		return FALSE;

	{
		const void* data = freerdp_settings_get_pointer(settings, FreeRDP_ServerCertificate);
		const UINT32 len = freerdp_settings_get_uint32(settings, FreeRDP_ServerCertificateLength);
		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_ServerCertificate, data, len))
			return FALSE;
	}
	if (settings->RdpServerCertificate)
	{
		rdpCertificate* cert = certificate_clone(settings->RdpServerCertificate);
		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_RdpServerCertificate, cert, 1))
			goto out_fail;
	}
	else
	{
		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_RdpServerCertificate, NULL, 0))
			goto out_fail;
	}

	if (settings->RdpServerRsaKey)
	{
		rdpRsaKey* key = key_clone(settings->RdpServerRsaKey);
		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_RdpServerRsaKey, key, 1))
			goto out_fail;
	}
	else
	{
		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_RdpServerRsaKey, NULL, 0))
			goto out_fail;
	}

	if (!freerdp_settings_set_uint32(_settings, FreeRDP_ChannelCount,
	                                 freerdp_settings_get_uint32(settings, FreeRDP_ChannelCount)))
		goto out_fail;
	if (!freerdp_settings_set_uint32(
	        _settings, FreeRDP_ChannelDefArraySize,
	        freerdp_settings_get_uint32(settings, FreeRDP_ChannelDefArraySize)))
		goto out_fail;

	const UINT32 defArraySize = freerdp_settings_get_uint32(settings, FreeRDP_ChannelDefArraySize);
	const CHANNEL_DEF* defArray = freerdp_settings_get_pointer(settings, FreeRDP_ChannelDefArray);
	if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_ChannelDefArray, defArray,
	                                      defArraySize))
		goto out_fail;

	{
		const UINT32 count = freerdp_settings_get_uint32(settings, FreeRDP_MonitorDefArraySize);
		const rdpMonitor* monitors =
		    freerdp_settings_get_pointer(settings, FreeRDP_MonitorDefArray);

		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_MonitorDefArray, monitors, count))
			goto out_fail;
	}

	if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_MonitorIds, NULL, 16))
		goto out_fail;

	const UINT32 monitorIdSize = freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorIds);
	const UINT32* monitorIds = freerdp_settings_get_pointer(settings, FreeRDP_MonitorIds);
	if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_MonitorIds, monitorIds, monitorIdSize))
		goto out_fail;

	_settings->OrderSupport = malloc(32);

	if (!_settings->OrderSupport)
		goto out_fail;

	if (!freerdp_capability_buffer_copy(_settings, settings))
		goto out_fail;
	CopyMemory(_settings->OrderSupport, settings->OrderSupport, 32);

	const UINT32 cellInfoSize =
	    freerdp_settings_get_uint32(settings, FreeRDP_BitmapCacheV2NumCells);
	const BITMAP_CACHE_V2_CELL_INFO* cellInfo =
	    freerdp_settings_get_pointer(settings, FreeRDP_BitmapCacheV2CellInfo);
	if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_BitmapCacheV2CellInfo, cellInfo,
	                                      cellInfoSize))
		goto out_fail;

	const UINT32 glyphCacheCount = 10;
	const GLYPH_CACHE_DEFINITION* glyphCache =
	    freerdp_settings_get_pointer(settings, FreeRDP_GlyphCache);
	if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_GlyphCache, glyphCache,
	                                      glyphCacheCount))
		goto out_fail;

	const UINT32 fragCacheCount = 1;
	const GLYPH_CACHE_DEFINITION* fragCache =
	    freerdp_settings_get_pointer(settings, FreeRDP_FragCache);
	if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_FragCache, fragCache, fragCacheCount))
		goto out_fail;

	if (!freerdp_settings_set_pointer_len(
	        _settings, FreeRDP_ClientAutoReconnectCookie,
	        freerdp_settings_get_pointer(settings, FreeRDP_ClientAutoReconnectCookie), 1))
		goto out_fail;
	if (!freerdp_settings_set_pointer_len(
	        _settings, FreeRDP_ServerAutoReconnectCookie,
	        freerdp_settings_get_pointer(settings, FreeRDP_ServerAutoReconnectCookie), 1))
		goto out_fail;

	const TIME_ZONE_INFORMATION* tz =
	    freerdp_settings_get_pointer(settings, FreeRDP_ClientTimeZone);
	if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_ClientTimeZone, tz, 1))
		goto out_fail;

	if (!freerdp_settings_set_uint32(
	        _settings, FreeRDP_RedirectionPasswordLength,
	        freerdp_settings_get_uint32(settings, FreeRDP_RedirectionPasswordLength)))
		goto out_fail;
	const UINT32 redirectionPasswordLength =
	    freerdp_settings_get_uint32(settings, FreeRDP_RedirectionPasswordLength);
	const BYTE* pwd = freerdp_settings_get_pointer(settings, FreeRDP_RedirectionPassword);
	if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_RedirectionPassword, pwd,
	                                      redirectionPasswordLength))
		goto out_fail;

	const UINT32 RedirectionTsvUrlLength =
	    freerdp_settings_get_uint32(settings, FreeRDP_RedirectionTsvUrlLength);
	const BYTE* RedirectionTsvUrl =
	    freerdp_settings_get_pointer(settings, FreeRDP_RedirectionTsvUrl);
	if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_RedirectionTsvUrl, RedirectionTsvUrl,
	                                      RedirectionTsvUrlLength))
		goto out_fail;

	freerdp_settings_set_uint32(
	    _settings, FreeRDP_TargetNetAddressCount,
	    freerdp_settings_get_uint32(settings, FreeRDP_TargetNetAddressCount));

	if (freerdp_settings_get_uint32(settings, FreeRDP_TargetNetAddressCount) > 0)
	{
		_settings->TargetNetAddresses = (char**)calloc(
		    freerdp_settings_get_uint32(settings, FreeRDP_TargetNetAddressCount), sizeof(char*));

		if (!_settings->TargetNetAddresses)
		{
			freerdp_settings_set_uint32(_settings, FreeRDP_TargetNetAddressCount, 0);
			goto out_fail;
		}

		for (index = 0;
		     index < freerdp_settings_get_uint32(settings, FreeRDP_TargetNetAddressCount); index++)
		{
			_settings->TargetNetAddresses[index] = _strdup(settings->TargetNetAddresses[index]);

			if (!_settings->TargetNetAddresses[index])
			{
				while (index)
					free(_settings->TargetNetAddresses[--index]);

				free(_settings->TargetNetAddresses);
				_settings->TargetNetAddresses = NULL;
				freerdp_settings_set_uint32(_settings, FreeRDP_TargetNetAddressCount, 0);
				goto out_fail;
			}
		}

		if (settings->TargetNetPorts)
		{
			_settings->TargetNetPorts = (UINT32*)calloc(
			    freerdp_settings_get_uint32(settings, FreeRDP_TargetNetAddressCount),
			    sizeof(UINT32));

			if (!_settings->TargetNetPorts)
				goto out_fail;

			for (index = 0;
			     index < freerdp_settings_get_uint32(settings, FreeRDP_TargetNetAddressCount);
			     index++)
				_settings->TargetNetPorts[index] = settings->TargetNetPorts[index];
		}
	}

	{
		const UINT32 len = freerdp_settings_get_uint32(_settings, FreeRDP_DeviceArraySize);
		const UINT32 count = freerdp_settings_get_uint32(settings, FreeRDP_DeviceCount);

		if (len < count)
			goto out_fail;
		if (!freerdp_settings_set_uint32(_settings, FreeRDP_DeviceCount, count))
			goto out_fail;
		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_DeviceArray, NULL, len))
			goto out_fail;

		for (index = 0; index < count; index++)
		{
			const RDPDR_DEVICE* argv =
			    freerdp_settings_get_pointer_array(settings, FreeRDP_DeviceArray, index);
			if (!freerdp_settings_set_pointer_array(_settings, FreeRDP_DeviceArray, index, argv))
				goto out_fail;
		}
	}
	{
		const UINT32 len = freerdp_settings_get_uint32(_settings, FreeRDP_StaticChannelArraySize);
		const UINT32 count = freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelCount);

		if (len < count)
			goto out_fail;
		if (!freerdp_settings_set_uint32(_settings, FreeRDP_StaticChannelCount, count))
			goto out_fail;
		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_StaticChannelArray, NULL, len))
			goto out_fail;

		for (index = 0; index < count; index++)
		{
			const ADDIN_ARGV* argv =
			    freerdp_settings_get_pointer_array(settings, FreeRDP_StaticChannelArray, index);
			if (!freerdp_settings_set_pointer_array(_settings, FreeRDP_StaticChannelArray, index,
			                                        argv))
				goto out_fail;
		}
	}
	{
		const UINT32 len = freerdp_settings_get_uint32(_settings, FreeRDP_DynamicChannelArraySize);
		const UINT32 count = freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelCount);

		if (len < count)
			goto out_fail;
		if (!freerdp_settings_set_uint32(_settings, FreeRDP_DynamicChannelCount, count))
			goto out_fail;
		if (!freerdp_settings_set_uint32(_settings, FreeRDP_DynamicChannelCount, count))
			goto out_fail;
		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_DynamicChannelArray, NULL, len))
			goto out_fail;

		for (index = 0; index < count; index++)
		{
			const ADDIN_ARGV* argv =
			    freerdp_settings_get_pointer_array(settings, FreeRDP_DynamicChannelArray, index);
			if (!freerdp_settings_set_pointer_array(_settings, FreeRDP_DynamicChannelArray, index,
			                                        argv))
				goto out_fail;
		}
	}

	rc = freerdp_settings_set_string(_settings, FreeRDP_ActionScript,
	                                 freerdp_settings_get_string(settings, FreeRDP_ActionScript));

	if (settings->XSelectionAtom)
		_settings->XSelectionAtom = _strdup(settings->XSelectionAtom);

out_fail:
	return rc;
}

BOOL freerdp_settings_copy(rdpSettings* _settings, const rdpSettings* settings)
{
	BOOL rc;

	if (!settings || !_settings)
		return FALSE;

	/* This is required to free all non string buffers */
	freerdp_settings_free_internal(_settings);
	/* This copies everything except allocated non string buffers. reset all allocated buffers to
	 * NULL to fix issues during cleanup */
	rc = freerdp_settings_clone_keys(_settings, settings);

	_settings->LoadBalanceInfo = NULL;
	_settings->ServerRandom = NULL;
	_settings->ClientRandom = NULL;
	_settings->ServerCertificate = NULL;
	_settings->RdpServerCertificate = NULL;
	_settings->RdpServerRsaKey = NULL;
	_settings->ChannelDefArray = NULL;
	_settings->MonitorDefArray = NULL;
	_settings->MonitorIds = NULL;
	_settings->OrderSupport = NULL;
	_settings->BitmapCacheV2CellInfo = NULL;
	_settings->GlyphCache = NULL;
	_settings->FragCache = NULL;
	_settings->ClientAutoReconnectCookie = NULL;
	_settings->ServerAutoReconnectCookie = NULL;
	_settings->ClientTimeZone = NULL;
	_settings->RedirectionPassword = NULL;
	_settings->RedirectionTsvUrl = NULL;
	_settings->TargetNetAddresses = NULL;
	_settings->DeviceArray = NULL;
	_settings->StaticChannelArray = NULL;
	_settings->DynamicChannelArray = NULL;
	_settings->ReceivedCapabilities = NULL;
	_settings->ReceivedCapabilityData = NULL;
	_settings->ReceivedCapabilityDataSizes = NULL;

	_settings->ServerLicenseProductIssuersCount = 0;
	_settings->ServerLicenseProductIssuers = NULL;

	_settings->XSelectionAtom = NULL;
	if (!rc)
		goto out_fail;

	/* Begin copying */
	if (!freerdp_settings_int_buffer_copy(_settings, settings))
		goto out_fail;
	return TRUE;
out_fail:
	freerdp_settings_free_internal(_settings);
	return FALSE;
}

rdpSettings* freerdp_settings_clone(const rdpSettings* settings)
{
	rdpSettings* _settings = (rdpSettings*)calloc(1, sizeof(rdpSettings));

	if (!freerdp_settings_copy(_settings, settings))
		goto out_fail;

	return _settings;
out_fail:
	freerdp_settings_free(_settings);
	return NULL;
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
