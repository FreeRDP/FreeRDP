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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/sysinfo.h>
#include <winpr/registry.h>

#include <freerdp/settings.h>

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 4244)
#endif

static const char client_dll[] = "C:\\Windows\\System32\\mstscax.dll";

#define REG_QUERY_DWORD_VALUE(_key, _subkey, _type, _value, _size, _result) \
	_size = sizeof(DWORD); \
	if (RegQueryValueEx(_key, _subkey, NULL, &_type, (BYTE*) &_value, &_size) == ERROR_SUCCESS) \
		_result = _value

#define REG_QUERY_BOOL_VALUE(_key, _subkey, _type, _value, _size, _result) \
	_size = sizeof(DWORD); \
	if (RegQueryValueEx(_key, _subkey, NULL, &_type, (BYTE*) &_value, &_size) == ERROR_SUCCESS) \
		_result = _value ? TRUE : FALSE

void settings_client_load_hkey_local_machine(rdpSettings* settings)
{
	HKEY hKey;
	LONG status;
	DWORD dwType;
	DWORD dwSize;
	DWORD dwValue;

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\FreeRDP\\Client"), 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		REG_QUERY_DWORD_VALUE(hKey, _T("DesktopWidth"), dwType, dwValue, dwSize, settings->DesktopWidth);
		REG_QUERY_DWORD_VALUE(hKey, _T("DesktopHeight"), dwType, dwValue, dwSize, settings->DesktopHeight);

		REG_QUERY_BOOL_VALUE(hKey, _T("Fullscreen"), dwType, dwValue, dwSize, settings->Fullscreen);
		REG_QUERY_DWORD_VALUE(hKey, _T("ColorDepth"), dwType, dwValue, dwSize, settings->ColorDepth);

		REG_QUERY_DWORD_VALUE(hKey, _T("KeyboardType"), dwType, dwValue, dwSize, settings->KeyboardType);
		REG_QUERY_DWORD_VALUE(hKey, _T("KeyboardSubType"), dwType, dwValue, dwSize, settings->KeyboardSubType);
		REG_QUERY_DWORD_VALUE(hKey, _T("KeyboardFunctionKeys"), dwType, dwValue, dwSize, settings->KeyboardFunctionKey);
		REG_QUERY_DWORD_VALUE(hKey, _T("KeyboardLayout"), dwType, dwValue, dwSize, settings->KeyboardLayout);

		REG_QUERY_BOOL_VALUE(hKey, _T("ExtSecurity"), dwType, dwValue, dwSize, settings->ExtSecurity);
		REG_QUERY_BOOL_VALUE(hKey, _T("NlaSecurity"), dwType, dwValue, dwSize, settings->NlaSecurity);
		REG_QUERY_BOOL_VALUE(hKey, _T("TlsSecurity"), dwType, dwValue, dwSize, settings->TlsSecurity);
		REG_QUERY_BOOL_VALUE(hKey, _T("RdpSecurity"), dwType, dwValue, dwSize, settings->RdpSecurity);

		REG_QUERY_BOOL_VALUE(hKey, _T("MstscCookieMode"), dwType, dwValue, dwSize, settings->MstscCookieMode);
		REG_QUERY_DWORD_VALUE(hKey, _T("CookieMaxLength"), dwType, dwValue, dwSize, settings->CookieMaxLength);

		REG_QUERY_BOOL_VALUE(hKey, _T("BitmapCache"), dwType, dwValue, dwSize, settings->BitmapCacheEnabled);

		REG_QUERY_BOOL_VALUE(hKey, _T("OffscreenBitmapCache"), dwType, dwValue, dwSize, settings->OffscreenSupportLevel);
		REG_QUERY_DWORD_VALUE(hKey, _T("OffscreenBitmapCacheSize"), dwType, dwValue, dwSize, settings->OffscreenCacheSize);
		REG_QUERY_DWORD_VALUE(hKey, _T("OffscreenBitmapCacheEntries"), dwType, dwValue, dwSize, settings->OffscreenCacheEntries);

		RegCloseKey(hKey);
	}

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\FreeRDP\\Client\\BitmapCacheV2"), 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		REG_QUERY_DWORD_VALUE(hKey, _T("NumCells"), dwType, dwValue, dwSize, settings->BitmapCacheV2NumCells);

		REG_QUERY_DWORD_VALUE(hKey, _T("Cell0NumEntries"), dwType, dwValue, dwSize, settings->BitmapCacheV2CellInfo[0].numEntries);
		REG_QUERY_BOOL_VALUE(hKey, _T("Cell0Persistent"), dwType, dwValue, dwSize, settings->BitmapCacheV2CellInfo[0].persistent);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cell1NumEntries"), dwType, dwValue, dwSize, settings->BitmapCacheV2CellInfo[1].numEntries);
		REG_QUERY_BOOL_VALUE(hKey, _T("Cell1Persistent"), dwType, dwValue, dwSize, settings->BitmapCacheV2CellInfo[1].persistent);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cell2NumEntries"), dwType, dwValue, dwSize, settings->BitmapCacheV2CellInfo[2].numEntries);
		REG_QUERY_BOOL_VALUE(hKey, _T("Cell2Persistent"), dwType, dwValue, dwSize, settings->BitmapCacheV2CellInfo[2].persistent);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cell3NumEntries"), dwType, dwValue, dwSize, settings->BitmapCacheV2CellInfo[3].numEntries);
		REG_QUERY_BOOL_VALUE(hKey, _T("Cell3Persistent"), dwType, dwValue, dwSize, settings->BitmapCacheV2CellInfo[3].persistent);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cell4NumEntries"), dwType, dwValue, dwSize, settings->BitmapCacheV2CellInfo[4].numEntries);
		REG_QUERY_BOOL_VALUE(hKey, _T("Cell4Persistent"), dwType, dwValue, dwSize, settings->BitmapCacheV2CellInfo[4].persistent);

		REG_QUERY_BOOL_VALUE(hKey, _T("AllowCacheWaitingList"), dwType, dwValue, dwSize, settings->AllowCacheWaitingList);

		RegCloseKey(hKey);
	}

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\FreeRDP\\Client\\GlyphCache"), 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		REG_QUERY_DWORD_VALUE(hKey, _T("SupportLevel"), dwType, dwValue, dwSize, settings->GlyphSupportLevel);

		REG_QUERY_DWORD_VALUE(hKey, _T("Cache0NumEntries"), dwType, dwValue, dwSize, settings->GlyphCache[0].cacheEntries);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache0MaxCellSize"), dwType, dwValue, dwSize, settings->GlyphCache[0].cacheMaximumCellSize);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache1NumEntries"), dwType, dwValue, dwSize, settings->GlyphCache[1].cacheEntries);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache1MaxCellSize"), dwType, dwValue, dwSize, settings->GlyphCache[1].cacheMaximumCellSize);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache2NumEntries"), dwType, dwValue, dwSize, settings->GlyphCache[2].cacheEntries);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache2MaxCellSize"), dwType, dwValue, dwSize, settings->GlyphCache[2].cacheMaximumCellSize);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache3NumEntries"), dwType, dwValue, dwSize, settings->GlyphCache[3].cacheEntries);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache3MaxCellSize"), dwType, dwValue, dwSize, settings->GlyphCache[3].cacheMaximumCellSize);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache4NumEntries"), dwType, dwValue, dwSize, settings->GlyphCache[4].cacheEntries);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache4MaxCellSize"), dwType, dwValue, dwSize, settings->GlyphCache[4].cacheMaximumCellSize);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache5NumEntries"), dwType, dwValue, dwSize, settings->GlyphCache[5].cacheEntries);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache5MaxCellSize"), dwType, dwValue, dwSize, settings->GlyphCache[5].cacheMaximumCellSize);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache6NumEntries"), dwType, dwValue, dwSize, settings->GlyphCache[6].cacheEntries);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache6MaxCellSize"), dwType, dwValue, dwSize, settings->GlyphCache[6].cacheMaximumCellSize);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache7NumEntries"), dwType, dwValue, dwSize, settings->GlyphCache[7].cacheEntries);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache7MaxCellSize"), dwType, dwValue, dwSize, settings->GlyphCache[7].cacheMaximumCellSize);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache8NumEntries"), dwType, dwValue, dwSize, settings->GlyphCache[8].cacheEntries);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache8MaxCellSize"), dwType, dwValue, dwSize, settings->GlyphCache[8].cacheMaximumCellSize);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache9NumEntries"), dwType, dwValue, dwSize, settings->GlyphCache[9].cacheEntries);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache9MaxCellSize"), dwType, dwValue, dwSize, settings->GlyphCache[9].cacheMaximumCellSize);

		REG_QUERY_DWORD_VALUE(hKey, _T("FragCacheNumEntries"), dwType, dwValue, dwSize, settings->FragCache->cacheEntries);
		REG_QUERY_DWORD_VALUE(hKey, _T("FragCacheMaxCellSize"), dwType, dwValue, dwSize, settings->FragCache->cacheMaximumCellSize);

		RegCloseKey(hKey);
	}

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\FreeRDP\\Client\\PointerCache"), 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		REG_QUERY_BOOL_VALUE(hKey, _T("LargePointer"), dwType, dwValue, dwSize, settings->LargePointerFlag);
		REG_QUERY_BOOL_VALUE(hKey, _T("ColorPointer"), dwType, dwValue, dwSize, settings->ColorPointerFlag);
		REG_QUERY_DWORD_VALUE(hKey, _T("PointerCacheSize"), dwType, dwValue, dwSize, settings->PointerCacheSize);

		RegCloseKey(hKey);
	}
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

	REG_QUERY_BOOL_VALUE(hKey, _T("ExtSecurity"), dwType, dwValue, dwSize, settings->ExtSecurity);
	REG_QUERY_BOOL_VALUE(hKey, _T("NlaSecurity"), dwType, dwValue, dwSize, settings->NlaSecurity);
	REG_QUERY_BOOL_VALUE(hKey, _T("TlsSecurity"), dwType, dwValue, dwSize, settings->TlsSecurity);
	REG_QUERY_BOOL_VALUE(hKey, _T("RdpSecurity"), dwType, dwValue, dwSize, settings->RdpSecurity);

	RegCloseKey(hKey);
}

void settings_load_hkey_local_machine(rdpSettings* settings)
{
	if (settings->ServerMode)
		settings_server_load_hkey_local_machine(settings);
	else
		settings_client_load_hkey_local_machine(settings);
}

void settings_get_computer_name(rdpSettings* settings)
{
	DWORD nSize = 0;

	GetComputerNameExA(ComputerNameNetBIOS, NULL, &nSize);
	settings->ComputerName = (char*) malloc(nSize);
	GetComputerNameExA(ComputerNameNetBIOS, settings->ComputerName, &nSize);
}

rdpSettings* freerdp_settings_new(DWORD flags)
{
	rdpSettings* settings;

	settings = (rdpSettings*) malloc(sizeof(rdpSettings));

	if (settings)
	{
		ZeroMemory(settings, sizeof(rdpSettings));

		settings->ServerMode = (flags & FREERDP_SETTINGS_SERVER_MODE) ? TRUE : FALSE;
		settings->WaitForOutputBufferFlush = TRUE;

		settings->DesktopWidth = 1024;
		settings->DesktopHeight = 768;
		settings->Workarea = FALSE;
		settings->Fullscreen = FALSE;
		settings->GrabKeyboard = TRUE;
		settings->Decorations = TRUE;
		settings->RdpVersion = 7;
		settings->ColorDepth = 16;
		settings->ExtSecurity = FALSE;
		settings->NlaSecurity = TRUE;
		settings->TlsSecurity = TRUE;
		settings->RdpSecurity = TRUE;
		settings->NegotiateSecurityLayer = TRUE;
		settings->RestrictedAdminModeRequired = FALSE;
		settings->MstscCookieMode = FALSE;
		settings->CookieMaxLength = DEFAULT_COOKIE_MAX_LENGTH;
		settings->ClientBuild = 2600;
		settings->KeyboardType = 4;
		settings->KeyboardSubType = 0;
		settings->KeyboardFunctionKey = 12;
		settings->KeyboardLayout = 0;
		settings->UseRdpSecurityLayer = FALSE;
		settings->SaltedChecksum = TRUE;
		settings->ServerPort = 3389;
		settings->GatewayPort = 443;
		settings->DesktopResize = TRUE;
		settings->ToggleFullscreen = TRUE;
		settings->DesktopPosX = 0;
		settings->DesktopPosY = 0;

		settings->PerformanceFlags = PERF_FLAG_NONE;
		settings->AllowFontSmoothing = FALSE;
		settings->AllowDesktopComposition = FALSE;
		settings->DisableWallpaper = TRUE;
		settings->DisableFullWindowDrag = TRUE;
		settings->DisableMenuAnims = TRUE;
		settings->DisableThemes = FALSE;
		settings->ConnectionType = CONNECTION_TYPE_LAN;

		settings->EncryptionMethods = ENCRYPTION_METHOD_NONE;
		settings->EncryptionLevel = ENCRYPTION_LEVEL_NONE;

		settings->CompressionEnabled = TRUE;

		if (settings->ServerMode)
			settings->CompressionLevel = PACKET_COMPR_TYPE_RDP61;
		else
			settings->CompressionLevel = PACKET_COMPR_TYPE_RDP61;

		settings->Authentication = TRUE;
		settings->AuthenticationOnly = FALSE;
		settings->CredentialsFromStdin = FALSE;
		settings->DisableCredentialsDelegation = FALSE;
		settings->AuthenticationLevel = 2;

		settings->ChannelCount = 0;
		settings->ChannelDefArraySize = 32;
		settings->ChannelDefArray = (CHANNEL_DEF*) calloc(settings->ChannelDefArraySize, sizeof(CHANNEL_DEF));

		settings->MonitorCount = 0;
		settings->MonitorDefArraySize = 32;
		settings->MonitorDefArray = (rdpMonitor*) calloc(settings->MonitorDefArraySize, sizeof(rdpMonitor));

		settings->MonitorIds = (UINT32*) calloc(16, sizeof(UINT32));

		settings_get_computer_name(settings);

		settings->ReceivedCapabilities = malloc(32);
		settings->OrderSupport = malloc(32);
		ZeroMemory(settings->ReceivedCapabilities, 32);
		ZeroMemory(settings->OrderSupport, 32);

		settings->OrderSupport[NEG_DSTBLT_INDEX] = TRUE;
		settings->OrderSupport[NEG_PATBLT_INDEX] = TRUE;
		settings->OrderSupport[NEG_SCRBLT_INDEX] = TRUE;
		settings->OrderSupport[NEG_OPAQUE_RECT_INDEX] = TRUE;
		settings->OrderSupport[NEG_DRAWNINEGRID_INDEX] = TRUE;
		settings->OrderSupport[NEG_MULTIDSTBLT_INDEX] = TRUE;
		settings->OrderSupport[NEG_MULTIPATBLT_INDEX] = TRUE;
		settings->OrderSupport[NEG_MULTISCRBLT_INDEX] = TRUE;
		settings->OrderSupport[NEG_MULTIOPAQUERECT_INDEX] = TRUE;
		settings->OrderSupport[NEG_MULTI_DRAWNINEGRID_INDEX] = TRUE;
		settings->OrderSupport[NEG_LINETO_INDEX] = TRUE;
		settings->OrderSupport[NEG_POLYLINE_INDEX] = TRUE;
		settings->OrderSupport[NEG_MEMBLT_INDEX] = TRUE;
		settings->OrderSupport[NEG_MEM3BLT_INDEX] = TRUE;
		settings->OrderSupport[NEG_SAVEBITMAP_INDEX] = TRUE;
		settings->OrderSupport[NEG_GLYPH_INDEX_INDEX] = TRUE;
		settings->OrderSupport[NEG_FAST_INDEX_INDEX] = TRUE;
		settings->OrderSupport[NEG_FAST_GLYPH_INDEX] = TRUE;
		settings->OrderSupport[NEG_POLYGON_SC_INDEX] = TRUE;
		settings->OrderSupport[NEG_POLYGON_CB_INDEX] = TRUE;
		settings->OrderSupport[NEG_ELLIPSE_SC_INDEX] = TRUE;
		settings->OrderSupport[NEG_ELLIPSE_CB_INDEX] = TRUE;

		settings->ClientHostname = malloc(32);
		settings->ClientProductId = malloc(32);
		ZeroMemory(settings->ClientHostname, 32);
		ZeroMemory(settings->ClientProductId, 32);

		gethostname(settings->ClientHostname, 31);
		settings->ClientHostname[31] = 0;

		settings->ColorPointerFlag = TRUE;
		settings->LargePointerFlag = TRUE;
		settings->PointerCacheSize = 20;
		settings->SoundBeepsEnabled = TRUE;

		settings->DrawGdiPlusEnabled = FALSE;

		settings->DrawAllowSkipAlpha = TRUE;
		settings->DrawAllowColorSubsampling = FALSE;
		settings->DrawAllowDynamicColorFidelity = FALSE;

		settings->FrameMarkerCommandEnabled = TRUE;
		settings->SurfaceFrameMarkerEnabled = TRUE;
		settings->BitmapCacheV3Enabled = FALSE;

		settings->BitmapCacheEnabled = TRUE;
		settings->BitmapCachePersistEnabled = FALSE;
		settings->AllowCacheWaitingList = TRUE;

		settings->BitmapCacheV2NumCells = 5;
		settings->BitmapCacheV2CellInfo = (BITMAP_CACHE_V2_CELL_INFO*) malloc(sizeof(BITMAP_CACHE_V2_CELL_INFO) * 6);
		settings->BitmapCacheV2CellInfo[0].numEntries = 600;
		settings->BitmapCacheV2CellInfo[0].persistent = FALSE;
		settings->BitmapCacheV2CellInfo[1].numEntries = 600;
		settings->BitmapCacheV2CellInfo[1].persistent = FALSE;
		settings->BitmapCacheV2CellInfo[2].numEntries = 2048;
		settings->BitmapCacheV2CellInfo[2].persistent = FALSE;
		settings->BitmapCacheV2CellInfo[3].numEntries = 4096;
		settings->BitmapCacheV2CellInfo[3].persistent = FALSE;
		settings->BitmapCacheV2CellInfo[4].numEntries = 2048;
		settings->BitmapCacheV2CellInfo[4].persistent = FALSE;

		settings->NoBitmapCompressionHeader = TRUE;

		settings->RefreshRect = TRUE;
		settings->SuppressOutput = TRUE;

		settings->GlyphSupportLevel = GLYPH_SUPPORT_FULL;
		settings->GlyphCache = malloc(sizeof(GLYPH_CACHE_DEFINITION) * 10);
		settings->FragCache = malloc(sizeof(GLYPH_CACHE_DEFINITION));
		settings->GlyphCache[0].cacheEntries = 254;
		settings->GlyphCache[0].cacheMaximumCellSize = 4;
		settings->GlyphCache[1].cacheEntries = 254;
		settings->GlyphCache[1].cacheMaximumCellSize = 4;
		settings->GlyphCache[2].cacheEntries = 254;
		settings->GlyphCache[2].cacheMaximumCellSize = 8;
		settings->GlyphCache[3].cacheEntries = 254;
		settings->GlyphCache[3].cacheMaximumCellSize = 8;
		settings->GlyphCache[4].cacheEntries = 254;
		settings->GlyphCache[4].cacheMaximumCellSize = 16;
		settings->GlyphCache[5].cacheEntries = 254;
		settings->GlyphCache[5].cacheMaximumCellSize = 32;
		settings->GlyphCache[6].cacheEntries = 254;
		settings->GlyphCache[6].cacheMaximumCellSize = 64;
		settings->GlyphCache[7].cacheEntries = 254;
		settings->GlyphCache[7].cacheMaximumCellSize = 128;
		settings->GlyphCache[8].cacheEntries = 254;
		settings->GlyphCache[8].cacheMaximumCellSize = 256;
		settings->GlyphCache[9].cacheEntries = 64;
		settings->GlyphCache[9].cacheMaximumCellSize = 256;
		settings->FragCache->cacheEntries = 256;
		settings->FragCache->cacheMaximumCellSize = 256;

		settings->OffscreenSupportLevel = TRUE;
		settings->OffscreenCacheSize = 7680;
		settings->OffscreenCacheEntries = 2000;

		settings->DrawNineGridCacheSize = 2560;
		settings->DrawNineGridCacheEntries = 256;

		settings->ClientDir = _strdup(client_dll);

		settings->RemoteAppNumIconCaches = 3;
		settings->RemoteAppNumIconCacheEntries = 12;

		settings->VirtualChannelChunkSize = CHANNEL_CHUNK_LENGTH;

		settings->MultifragMaxRequestSize = 0xFFFF;

		settings->GatewayUseSameCredentials = FALSE;
		settings->GatewayBypassLocal = FALSE;

		settings->FastPathInput = TRUE;
		settings->FastPathOutput = TRUE;

		settings->FrameAcknowledge = 2;
		settings->MouseMotion = TRUE;

		settings->NSCodecColorLossLevel = 3;
		settings->NSCodecAllowSubsampling = TRUE;
		settings->NSCodecAllowDynamicColorFidelity = TRUE;

		settings->AutoReconnectionEnabled = FALSE;
		settings->AutoReconnectMaxRetries = 20;

		settings->GfxThinClient = TRUE;
		settings->GfxSmallCache = FALSE;
		settings->GfxProgressive = FALSE;
		settings->GfxProgressiveV2 = FALSE;
		settings->GfxH264 = FALSE;

		settings->ClientAutoReconnectCookie = (ARC_CS_PRIVATE_PACKET*) malloc(sizeof(ARC_CS_PRIVATE_PACKET));
		settings->ServerAutoReconnectCookie = (ARC_SC_PRIVATE_PACKET*) malloc(sizeof(ARC_SC_PRIVATE_PACKET));
		ZeroMemory(settings->ClientAutoReconnectCookie, sizeof(ARC_CS_PRIVATE_PACKET));
		ZeroMemory(settings->ServerAutoReconnectCookie, sizeof(ARC_SC_PRIVATE_PACKET));

		settings->ClientTimeZone = (TIME_ZONE_INFO*) malloc(sizeof(TIME_ZONE_INFO));
		ZeroMemory(settings->ClientTimeZone, sizeof(TIME_ZONE_INFO));

		settings->DeviceArraySize = 16;
		settings->DeviceArray = (RDPDR_DEVICE**) malloc(sizeof(RDPDR_DEVICE*) * settings->DeviceArraySize);
		ZeroMemory(settings->DeviceArray, sizeof(RDPDR_DEVICE*) * settings->DeviceArraySize);

		settings->StaticChannelArraySize = 16;
		settings->StaticChannelArray = (ADDIN_ARGV**)
				malloc(sizeof(ADDIN_ARGV*) * settings->StaticChannelArraySize);
		ZeroMemory(settings->StaticChannelArray, sizeof(ADDIN_ARGV*) * settings->StaticChannelArraySize);

		settings->DynamicChannelArraySize = 16;
		settings->DynamicChannelArray = (ADDIN_ARGV**)
				malloc(sizeof(ADDIN_ARGV*) * settings->DynamicChannelArraySize);
		ZeroMemory(settings->DynamicChannelArray, sizeof(ADDIN_ARGV*) * settings->DynamicChannelArraySize);

		settings->HomePath = GetKnownPath(KNOWN_PATH_HOME);
		settings->ConfigPath = GetKnownSubPath(KNOWN_PATH_XDG_CONFIG_HOME, "freerdp");

		settings_load_hkey_local_machine(settings);

		settings->SettingsModified = (BYTE*) malloc(sizeof(rdpSettings) / 8 );
		ZeroMemory(settings->SettingsModified, sizeof(rdpSettings) / 8);
	}

	return settings;
}

rdpSettings* freerdp_settings_clone(rdpSettings* settings)
{
	UINT32 index;
	rdpSettings* _settings;

	_settings = (rdpSettings*) calloc(1, sizeof(rdpSettings));

	if (_settings)
	{
		CopyMemory(_settings, settings, sizeof(rdpSettings));

		/**
		  * Generated Code
		  */

		/* char* values */

		_settings->ServerHostname = _strdup(settings->ServerHostname); /* 20 */
		_settings->Username = _strdup(settings->Username); /* 21 */
		_settings->Password = _strdup(settings->Password); /* 22 */
		_settings->Domain = _strdup(settings->Domain); /* 23 */
		_settings->PasswordHash = _strdup(settings->PasswordHash); /* 24 */
		_settings->ClientHostname = NULL; /* 134 */
		_settings->ClientProductId = NULL; /* 135 */
		_settings->AlternateShell = _strdup(settings->AlternateShell); /* 640 */
		_settings->ShellWorkingDirectory = _strdup(settings->ShellWorkingDirectory); /* 641 */
		_settings->ClientAddress = _strdup(settings->ClientAddress); /* 769 */
		_settings->ClientDir = _strdup(settings->ClientDir); /* 770 */
		_settings->DynamicDSTTimeZoneKeyName = _strdup(settings->DynamicDSTTimeZoneKeyName); /* 897 */
		_settings->RemoteAssistanceSessionId = _strdup(settings->RemoteAssistanceSessionId); /* 1025 */
		_settings->RemoteAssistancePassStub = _strdup(settings->RemoteAssistancePassStub); /* 1026 */
		_settings->RemoteAssistancePassword = _strdup(settings->RemoteAssistancePassword); /* 1027 */
		_settings->RemoteAssistanceRCTicket = _strdup(settings->RemoteAssistanceRCTicket); /* 1028 */
		_settings->AuthenticationServiceClass = _strdup(settings->AuthenticationServiceClass); /* 1098 */
		_settings->AllowedTlsCiphers = _strdup(settings->AllowedTlsCiphers); /* 1101 */
		_settings->PreconnectionBlob = _strdup(settings->PreconnectionBlob); /* 1155 */
		_settings->KerberosKdc = _strdup(settings->KerberosKdc); /* 1344 */
		_settings->KerberosRealm = _strdup(settings->KerberosRealm); /* 1345 */
		_settings->CertificateName = _strdup(settings->CertificateName); /* 1409 */
		_settings->CertificateFile = _strdup(settings->CertificateFile); /* 1410 */
		_settings->PrivateKeyFile = _strdup(settings->PrivateKeyFile); /* 1411 */
		_settings->RdpKeyFile = _strdup(settings->RdpKeyFile); /* 1412 */
		_settings->WindowTitle = _strdup(settings->WindowTitle); /* 1542 */
		_settings->WmClass = _strdup(settings->WmClass); /* 1549 */
		_settings->ComputerName = _strdup(settings->ComputerName); /* 1664 */
		_settings->ConnectionFile = _strdup(settings->ConnectionFile); /* 1728 */
		_settings->AssistanceFile = _strdup(settings->AssistanceFile); /* 1729 */
		_settings->HomePath = _strdup(settings->HomePath); /* 1792 */
		_settings->ConfigPath = _strdup(settings->ConfigPath); /* 1793 */
		_settings->CurrentPath = _strdup(settings->CurrentPath); /* 1794 */
		_settings->DumpRemoteFxFile = _strdup(settings->DumpRemoteFxFile); /* 1858 */
		_settings->PlayRemoteFxFile = _strdup(settings->PlayRemoteFxFile); /* 1859 */
		_settings->GatewayHostname = _strdup(settings->GatewayHostname); /* 1986 */
		_settings->GatewayUsername = _strdup(settings->GatewayUsername); /* 1987 */
		_settings->GatewayPassword = _strdup(settings->GatewayPassword); /* 1988 */
		_settings->GatewayDomain = _strdup(settings->GatewayDomain); /* 1989 */
		_settings->RemoteApplicationName = _strdup(settings->RemoteApplicationName); /* 2113 */
		_settings->RemoteApplicationIcon = _strdup(settings->RemoteApplicationIcon); /* 2114 */
		_settings->RemoteApplicationProgram = _strdup(settings->RemoteApplicationProgram); /* 2115 */
		_settings->RemoteApplicationFile = _strdup(settings->RemoteApplicationFile); /* 2116 */
		_settings->RemoteApplicationGuid = _strdup(settings->RemoteApplicationGuid); /* 2117 */
		_settings->RemoteApplicationCmdLine = _strdup(settings->RemoteApplicationCmdLine); /* 2118 */
		_settings->ImeFileName = _strdup(settings->ImeFileName); /* 2628 */
		_settings->DrivesToRedirect = _strdup(settings->DrivesToRedirect); /* 4290 */

		/**
		  * Manual Code
		  */

		_settings->LoadBalanceInfo = NULL;
		_settings->LoadBalanceInfoLength = 0;
		_settings->TargetNetAddress = NULL;
		_settings->RedirectionTargetFQDN = NULL;
		_settings->RedirectionTargetNetBiosName = NULL;
		_settings->RedirectionUsername = NULL;
		_settings->RedirectionDomain = NULL;
		_settings->RedirectionPassword = NULL;
		_settings->RedirectionPasswordLength = 0;
		_settings->RedirectionTsvUrl = NULL;
		_settings->RedirectionTsvUrlLength = 0;
		_settings->TargetNetAddressCount = 0;
		_settings->TargetNetAddresses = NULL;

		if (settings->LoadBalanceInfo && settings->LoadBalanceInfoLength)
		{
			_settings->LoadBalanceInfo = (BYTE*) calloc(1, settings->LoadBalanceInfoLength + 2);
			CopyMemory(_settings->LoadBalanceInfo, settings->LoadBalanceInfo, settings->LoadBalanceInfoLength);
			_settings->LoadBalanceInfoLength = settings->LoadBalanceInfoLength;
		}

		if (_settings->ServerRandomLength)
		{
			_settings->ServerRandom = (BYTE*) malloc(_settings->ServerRandomLength);
			CopyMemory(_settings->ServerRandom, settings->ServerRandom, _settings->ServerRandomLength);
			_settings->ServerRandomLength = settings->ServerRandomLength;
		}

		if (_settings->ClientRandomLength)
		{
			_settings->ClientRandom = (BYTE*) malloc(_settings->ClientRandomLength);
			CopyMemory(_settings->ClientRandom, settings->ClientRandom, _settings->ClientRandomLength);
			_settings->ClientRandomLength = settings->ClientRandomLength;
		}

		if (settings->RdpServerCertificate)
		{
			_settings->RdpServerCertificate = certificate_clone(settings->RdpServerCertificate);
		}

		_settings->ChannelCount = settings->ChannelCount;
		_settings->ChannelDefArraySize = settings->ChannelDefArraySize;
		_settings->ChannelDefArray = (CHANNEL_DEF*) malloc(sizeof(CHANNEL_DEF) * settings->ChannelDefArraySize);
		CopyMemory(_settings->ChannelDefArray, settings->ChannelDefArray, sizeof(CHANNEL_DEF) * settings->ChannelDefArraySize);

		_settings->MonitorCount = settings->MonitorCount;
		_settings->MonitorDefArraySize = settings->MonitorDefArraySize;
		_settings->MonitorDefArray = (rdpMonitor*) malloc(sizeof(rdpMonitor) * settings->MonitorDefArraySize);
		CopyMemory(_settings->MonitorDefArray, settings->MonitorDefArray, sizeof(rdpMonitor) * settings->MonitorDefArraySize);

		_settings->MonitorIds = (UINT32*) calloc(16, sizeof(UINT32));
		CopyMemory(_settings->MonitorIds, settings->MonitorIds, 16 * sizeof(UINT32));

		_settings->ReceivedCapabilities = malloc(32);
		_settings->OrderSupport = malloc(32);
		CopyMemory(_settings->ReceivedCapabilities, settings->ReceivedCapabilities, 32);
		CopyMemory(_settings->OrderSupport, settings->OrderSupport, 32);

		_settings->ClientHostname = malloc(32);
		_settings->ClientProductId = malloc(32);
		CopyMemory(_settings->ClientHostname, settings->ClientHostname, 32);
		CopyMemory(_settings->ClientProductId, settings->ClientProductId, 32);

		_settings->BitmapCacheV2CellInfo = (BITMAP_CACHE_V2_CELL_INFO*) malloc(sizeof(BITMAP_CACHE_V2_CELL_INFO) * 6);
		CopyMemory(_settings->BitmapCacheV2CellInfo, settings->BitmapCacheV2CellInfo, sizeof(BITMAP_CACHE_V2_CELL_INFO) * 6);

		_settings->GlyphCache = malloc(sizeof(GLYPH_CACHE_DEFINITION) * 10);
		_settings->FragCache = malloc(sizeof(GLYPH_CACHE_DEFINITION));
		CopyMemory(_settings->GlyphCache, settings->GlyphCache, sizeof(GLYPH_CACHE_DEFINITION) * 10);
		CopyMemory(_settings->FragCache, settings->FragCache, sizeof(GLYPH_CACHE_DEFINITION));

		_settings->ClientAutoReconnectCookie = (ARC_CS_PRIVATE_PACKET*) malloc(sizeof(ARC_CS_PRIVATE_PACKET));
		_settings->ServerAutoReconnectCookie = (ARC_SC_PRIVATE_PACKET*) malloc(sizeof(ARC_SC_PRIVATE_PACKET));
		CopyMemory(_settings->ClientAutoReconnectCookie, settings->ClientAutoReconnectCookie, sizeof(ARC_CS_PRIVATE_PACKET));
		CopyMemory(_settings->ServerAutoReconnectCookie, settings->ServerAutoReconnectCookie, sizeof(ARC_SC_PRIVATE_PACKET));

		_settings->ClientTimeZone = (TIME_ZONE_INFO*) malloc(sizeof(TIME_ZONE_INFO));
		CopyMemory(_settings->ClientTimeZone, settings->ClientTimeZone, sizeof(TIME_ZONE_INFO));

		_settings->TargetNetAddressCount = settings->TargetNetAddressCount;

		if (settings->TargetNetAddressCount > 0)
		{
			_settings->TargetNetAddresses = (char**) malloc(sizeof(char*) * settings->TargetNetAddressCount);

			for (index = 0; index < settings->TargetNetAddressCount; index++)
				_settings->TargetNetAddresses[index] = _strdup(settings->TargetNetAddresses[index]);
		}

		_settings->DeviceCount = settings->DeviceCount;
		_settings->DeviceArraySize = settings->DeviceArraySize;
		_settings->DeviceArray = (RDPDR_DEVICE**) malloc(sizeof(RDPDR_DEVICE*) * _settings->DeviceArraySize);
		ZeroMemory(_settings->DeviceArray, sizeof(RDPDR_DEVICE*) * _settings->DeviceArraySize);

		for (index = 0; index < _settings->DeviceCount; index++)
		{
			_settings->DeviceArray[index] = freerdp_device_clone(settings->DeviceArray[index]);
		}

		_settings->StaticChannelCount = settings->StaticChannelCount;
		_settings->StaticChannelArraySize = settings->StaticChannelArraySize;
		_settings->StaticChannelArray = (ADDIN_ARGV**) calloc(_settings->StaticChannelArraySize, sizeof(ADDIN_ARGV*));

		for (index = 0; index < _settings->StaticChannelCount; index++)
		{
			_settings->StaticChannelArray[index] = freerdp_static_channel_clone(settings->StaticChannelArray[index]);
		}

		_settings->DynamicChannelCount = settings->DynamicChannelCount;
		_settings->DynamicChannelArraySize = settings->DynamicChannelArraySize;
		_settings->DynamicChannelArray = (ADDIN_ARGV**) calloc(_settings->DynamicChannelArraySize, sizeof(ADDIN_ARGV*));

		for (index = 0; index < _settings->DynamicChannelCount; index++)
		{
			_settings->DynamicChannelArray[index] = freerdp_dynamic_channel_clone(settings->DynamicChannelArray[index]);
		}

		_settings->SettingsModified = (BYTE*) malloc(sizeof(rdpSettings) / 8);
		ZeroMemory(_settings->SettingsModified, sizeof(rdpSettings) / 8);
	}

	return _settings;
}

void freerdp_settings_free(rdpSettings* settings)
{
	if (settings)
	{
		free(settings->ServerHostname);
		free(settings->Username);
		free(settings->Password);
		free(settings->Domain);
		free(settings->PasswordHash);
		free(settings->AlternateShell);
		free(settings->ShellWorkingDirectory);
		free(settings->ComputerName);
		free(settings->ChannelDefArray);
		free(settings->MonitorDefArray);
		free(settings->MonitorIds);
		free(settings->ClientAddress);
		free(settings->ClientDir);
		free(settings->AllowedTlsCiphers);
		free(settings->CertificateFile);
		free(settings->PrivateKeyFile);
		free(settings->ConnectionFile);
		free(settings->AssistanceFile);
		free(settings->ReceivedCapabilities);
		free(settings->OrderSupport);
		free(settings->ClientHostname);
		free(settings->ClientProductId);
		free(settings->ServerRandom);
		free(settings->ClientRandom);
		free(settings->ServerCertificate);
		free(settings->RdpKeyFile);
		certificate_free(settings->RdpServerCertificate);
		free(settings->ClientAutoReconnectCookie);
		free(settings->ServerAutoReconnectCookie);
		free(settings->ClientTimeZone);
		free(settings->BitmapCacheV2CellInfo);
		free(settings->GlyphCache);
		free(settings->FragCache);
		key_free(settings->RdpServerRsaKey);
		free(settings->ConfigPath);
		free(settings->CurrentPath);
		free(settings->HomePath);
		free(settings->LoadBalanceInfo);
		free(settings->TargetNetAddress);
		free(settings->RedirectionTargetFQDN);
		free(settings->RedirectionTargetNetBiosName);
		free(settings->RedirectionUsername);
		free(settings->RedirectionDomain);
		free(settings->RedirectionPassword);
		free(settings->RedirectionTsvUrl);
		free(settings->RemoteAssistanceSessionId);
		free(settings->AuthenticationServiceClass);
		free(settings->GatewayHostname);
		free(settings->GatewayUsername);
		free(settings->GatewayPassword);
		free(settings->GatewayDomain);
		freerdp_target_net_addresses_free(settings);
		freerdp_device_collection_free(settings);
		freerdp_static_channel_collection_free(settings);
		freerdp_dynamic_channel_collection_free(settings);
		free(settings->SettingsModified);
		free(settings);
	}
}

#ifdef _WIN32
#pragma warning(pop)
#endif

