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
		settings->DisableEncryption = FALSE;
		settings->SaltedChecksum = TRUE;
		settings->ServerPort = 3389;
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

		settings->Authentication = TRUE;
		settings->AuthenticationOnly = FALSE;
		settings->CredentialsFromStdin = FALSE;

		settings->ChannelCount = 0;
		settings->ChannelDefArraySize = 32;
		settings->ChannelDefArray = (rdpChannel*) malloc(sizeof(rdpChannel) * settings->ChannelDefArraySize);
		ZeroMemory(settings->ChannelDefArray, sizeof(rdpChannel) * settings->ChannelDefArraySize);

		settings->MonitorCount = 0;
		settings->MonitorDefArraySize = 32;
		settings->MonitorDefArray = (rdpMonitor*) malloc(sizeof(rdpMonitor) * settings->MonitorDefArraySize);
		ZeroMemory(settings->MonitorDefArray, sizeof(rdpMonitor) * settings->MonitorDefArraySize);

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

		settings->FrameMarkerCommandEnabled = FALSE;
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

		settings->FastPathInput = TRUE;
		settings->FastPathOutput = TRUE;

		settings->FrameAcknowledge = 2;
		settings->MouseMotion = TRUE;

		settings->AutoReconnectionEnabled = TRUE;
		settings->AutoReconnectMaxRetries = 20;

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
	int index;
	rdpSettings* _settings;

	_settings = (rdpSettings*) malloc(sizeof(rdpSettings));

	if (_settings)
	{
		ZeroMemory(_settings, sizeof(rdpSettings));

		/**
		  * Generated Code
		  */

		/* char* values */

		_settings->ServerHostname = _strdup(settings->ServerHostname); /* 20 */
		_settings->Username = _strdup(settings->Username); /* 21 */
		_settings->Password = _strdup(settings->Password); /* 22 */
		_settings->Domain = _strdup(settings->Domain); /* 23 */
		_settings->PasswordHash = _strdup(settings->PasswordHash); /* 24 */
		//_settings->ClientHostname = _strdup(settings->ClientHostname); /* 134 */
		//_settings->ClientProductId = _strdup(settings->ClientProductId); /* 135 */
		_settings->AlternateShell = _strdup(settings->AlternateShell); /* 640 */
		_settings->ShellWorkingDirectory = _strdup(settings->ShellWorkingDirectory); /* 641 */
		_settings->ClientAddress = _strdup(settings->ClientAddress); /* 769 */
		_settings->ClientDir = _strdup(settings->ClientDir); /* 770 */
		_settings->DynamicDSTTimeZoneKeyName = _strdup(settings->DynamicDSTTimeZoneKeyName); /* 897 */
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

		/* UINT32 values */

		_settings->ShareId = settings->ShareId; /* 17 */
		_settings->PduSource = settings->PduSource; /* 18 */
		_settings->ServerPort = settings->ServerPort; /* 19 */
		_settings->RdpVersion = settings->RdpVersion; /* 128 */
		_settings->DesktopWidth = settings->DesktopWidth; /* 129 */
		_settings->DesktopHeight = settings->DesktopHeight; /* 130 */
		_settings->ColorDepth = settings->ColorDepth; /* 131 */
		_settings->ConnectionType = settings->ConnectionType; /* 132 */
		_settings->ClientBuild = settings->ClientBuild; /* 133 */
		_settings->EarlyCapabilityFlags = settings->EarlyCapabilityFlags; /* 136 */
		_settings->EncryptionMethods = settings->EncryptionMethods; /* 193 */
		_settings->ExtEncryptionMethods = settings->ExtEncryptionMethods; /* 194 */
		_settings->EncryptionLevel = settings->EncryptionLevel; /* 195 */
		_settings->ServerRandomLength = settings->ServerRandomLength; /* 197 */
		_settings->ServerCertificateLength = settings->ServerCertificateLength; /* 199 */
		_settings->ChannelCount = settings->ChannelCount; /* 256 */
		_settings->ChannelDefArraySize = settings->ChannelDefArraySize; /* 257 */
		_settings->ClusterInfoFlags = settings->ClusterInfoFlags; /* 320 */
		_settings->RedirectedSessionId = settings->RedirectedSessionId; /* 321 */
		_settings->MonitorDefArraySize = settings->MonitorDefArraySize; /* 385 */
		_settings->DesktopPosX = settings->DesktopPosX; /* 390 */
		_settings->DesktopPosY = settings->DesktopPosY; /* 391 */
		_settings->NumMonitorIds = settings->NumMonitorIds; /* 394 */
		_settings->MultitransportFlags = settings->MultitransportFlags; /* 512 */
		_settings->AutoReconnectMaxRetries = settings->AutoReconnectMaxRetries; /* 833 */
		_settings->PerformanceFlags = settings->PerformanceFlags; /* 960 */
		_settings->RequestedProtocols = settings->RequestedProtocols; /* 1093 */
		_settings->SelectedProtocol = settings->SelectedProtocol; /* 1094 */
		_settings->NegotiationFlags = settings->NegotiationFlags; /* 1095 */
		_settings->CookieMaxLength = settings->CookieMaxLength; /* 1153 */
		_settings->PreconnectionId = settings->PreconnectionId; /* 1154 */
		_settings->RedirectionFlags = settings->RedirectionFlags; /* 1216 */
		_settings->LoadBalanceInfoLength = settings->LoadBalanceInfoLength; /* 1218 */
		_settings->RedirectionPasswordLength = settings->RedirectionPasswordLength; /* 1224 */
		_settings->RedirectionTsvUrlLength = settings->RedirectionTsvUrlLength; /* 1230 */
		_settings->TargetNetAddressCount = settings->TargetNetAddressCount; /* 1231 */
		_settings->Password51Length = settings->Password51Length; /* 1281 */
		_settings->PercentScreen = settings->PercentScreen; /* 1538 */
		_settings->GatewayUsageMethod = settings->GatewayUsageMethod; /* 1984 */
		_settings->GatewayPort = settings->GatewayPort; /* 1985 */
		_settings->GatewayCredentialsSource = settings->GatewayCredentialsSource; /* 1990 */
		_settings->RemoteApplicationExpandCmdLine = settings->RemoteApplicationExpandCmdLine; /* 2119 */
		_settings->RemoteApplicationExpandWorkingDir = settings->RemoteApplicationExpandWorkingDir; /* 2120 */
		_settings->RemoteAppNumIconCaches = settings->RemoteAppNumIconCaches; /* 2122 */
		_settings->RemoteAppNumIconCacheEntries = settings->RemoteAppNumIconCacheEntries; /* 2123 */
		_settings->ReceivedCapabilitiesSize = settings->ReceivedCapabilitiesSize; /* 2241 */
		_settings->OsMajorType = settings->OsMajorType; /* 2304 */
		_settings->OsMinorType = settings->OsMinorType; /* 2305 */
		_settings->BitmapCacheVersion = settings->BitmapCacheVersion; /* 2498 */
		_settings->BitmapCacheV2NumCells = settings->BitmapCacheV2NumCells; /* 2501 */
		_settings->PointerCacheSize = settings->PointerCacheSize; /* 2561 */
		_settings->KeyboardLayout = settings->KeyboardLayout; /* 2624 */
		_settings->KeyboardType = settings->KeyboardType; /* 2625 */
		_settings->KeyboardSubType = settings->KeyboardSubType; /* 2626 */
		_settings->KeyboardFunctionKey = settings->KeyboardFunctionKey; /* 2627 */
		_settings->BrushSupportLevel = settings->BrushSupportLevel; /* 2688 */
		_settings->GlyphSupportLevel = settings->GlyphSupportLevel; /* 2752 */
		_settings->OffscreenSupportLevel = settings->OffscreenSupportLevel; /* 2816 */
		_settings->OffscreenCacheSize = settings->OffscreenCacheSize; /* 2817 */
		_settings->OffscreenCacheEntries = settings->OffscreenCacheEntries; /* 2818 */
		_settings->VirtualChannelCompressionFlags = settings->VirtualChannelCompressionFlags; /* 2880 */
		_settings->VirtualChannelChunkSize = settings->VirtualChannelChunkSize; /* 2881 */
		_settings->MultifragMaxRequestSize = settings->MultifragMaxRequestSize; /* 3328 */
		_settings->LargePointerFlag = settings->LargePointerFlag; /* 3392 */
		_settings->CompDeskSupportLevel = settings->CompDeskSupportLevel; /* 3456 */
		_settings->RemoteFxCodecId = settings->RemoteFxCodecId; /* 3650 */
		_settings->RemoteFxCodecMode = settings->RemoteFxCodecMode; /* 3651 */
		_settings->RemoteFxCaptureFlags = settings->RemoteFxCaptureFlags; /* 3653 */
		_settings->NSCodecId = settings->NSCodecId; /* 3713 */
		_settings->FrameAcknowledge = settings->FrameAcknowledge; /* 3714 */
		_settings->JpegCodecId = settings->JpegCodecId; /* 3777 */
		_settings->JpegQuality = settings->JpegQuality; /* 3778 */
		_settings->BitmapCacheV3CodecId = settings->BitmapCacheV3CodecId; /* 3904 */
		_settings->DrawNineGridCacheSize = settings->DrawNineGridCacheSize; /* 3969 */
		_settings->DrawNineGridCacheEntries = settings->DrawNineGridCacheEntries; /* 3970 */
		_settings->DeviceCount = settings->DeviceCount; /* 4161 */
		_settings->DeviceArraySize = settings->DeviceArraySize; /* 4162 */
		_settings->StaticChannelCount = settings->StaticChannelCount; /* 4928 */
		_settings->StaticChannelArraySize = settings->StaticChannelArraySize; /* 4929 */
		_settings->DynamicChannelCount = settings->DynamicChannelCount; /* 5056 */
		_settings->DynamicChannelArraySize = settings->DynamicChannelArraySize; /* 5057 */

		/* BOOL values */

		_settings->ServerMode = settings->ServerMode; /* 16 */
		_settings->NetworkAutoDetect = settings->NetworkAutoDetect; /* 137 */
		_settings->SupportAsymetricKeys = settings->SupportAsymetricKeys; /* 138 */
		_settings->SupportErrorInfoPdu = settings->SupportErrorInfoPdu; /* 139 */
		_settings->SupportStatusInfoPdu = settings->SupportStatusInfoPdu; /* 140 */
		_settings->SupportMonitorLayoutPdu = settings->SupportMonitorLayoutPdu; /* 141 */
		_settings->SupportGraphicsPipeline = settings->SupportGraphicsPipeline; /* 142 */
		_settings->SupportDynamicTimeZone = settings->SupportDynamicTimeZone; /* 143 */
		_settings->DisableEncryption = settings->DisableEncryption; /* 192 */
		_settings->ConsoleSession = settings->ConsoleSession; /* 322 */
		_settings->SpanMonitors = settings->SpanMonitors; /* 387 */
		_settings->UseMultimon = settings->UseMultimon; /* 388 */
		_settings->ForceMultimon = settings->ForceMultimon; /* 389 */
		_settings->ListMonitors = settings->ListMonitors; /* 392 */
		_settings->AutoLogonEnabled = settings->AutoLogonEnabled; /* 704 */
		_settings->CompressionEnabled = settings->CompressionEnabled; /* 705 */
		_settings->DisableCtrlAltDel = settings->DisableCtrlAltDel; /* 706 */
		_settings->EnableWindowsKey = settings->EnableWindowsKey; /* 707 */
		_settings->MaximizeShell = settings->MaximizeShell; /* 708 */
		_settings->LogonNotify = settings->LogonNotify; /* 709 */
		_settings->LogonErrors = settings->LogonErrors; /* 710 */
		_settings->MouseAttached = settings->MouseAttached; /* 711 */
		_settings->MouseHasWheel = settings->MouseHasWheel; /* 712 */
		_settings->RemoteConsoleAudio = settings->RemoteConsoleAudio; /* 713 */
		_settings->AudioPlayback = settings->AudioPlayback; /* 714 */
		_settings->AudioCapture = settings->AudioCapture; /* 715 */
		_settings->VideoDisable = settings->VideoDisable; /* 716 */
		_settings->PasswordIsSmartcardPin = settings->PasswordIsSmartcardPin; /* 717 */
		_settings->UsingSavedCredentials = settings->UsingSavedCredentials; /* 718 */
		_settings->ForceEncryptedCsPdu = settings->ForceEncryptedCsPdu; /* 719 */
		_settings->HiDefRemoteApp = settings->HiDefRemoteApp; /* 720 */
		_settings->IPv6Enabled = settings->IPv6Enabled; /* 768 */
		_settings->AutoReconnectionEnabled = settings->AutoReconnectionEnabled; /* 832 */
		_settings->DynamicDaylightTimeDisabled = settings->DynamicDaylightTimeDisabled; /* 898 */
		_settings->AllowFontSmoothing = settings->AllowFontSmoothing; /* 961 */
		_settings->DisableWallpaper = settings->DisableWallpaper; /* 962 */
		_settings->DisableFullWindowDrag = settings->DisableFullWindowDrag; /* 963 */
		_settings->DisableMenuAnims = settings->DisableMenuAnims; /* 964 */
		_settings->DisableThemes = settings->DisableThemes; /* 965 */
		_settings->DisableCursorShadow = settings->DisableCursorShadow; /* 966 */
		_settings->DisableCursorBlinking = settings->DisableCursorBlinking; /* 967 */
		_settings->AllowDesktopComposition = settings->AllowDesktopComposition; /* 968 */
		_settings->TlsSecurity = settings->TlsSecurity; /* 1088 */
		_settings->NlaSecurity = settings->NlaSecurity; /* 1089 */
		_settings->RdpSecurity = settings->RdpSecurity; /* 1090 */
		_settings->ExtSecurity = settings->ExtSecurity; /* 1091 */
		_settings->Authentication = settings->Authentication; /* 1092 */
		_settings->NegotiateSecurityLayer = settings->NegotiateSecurityLayer; /* 1096 */
		_settings->RestrictedAdminModeRequired = settings->RestrictedAdminModeRequired; /* 1097 */
		_settings->MstscCookieMode = settings->MstscCookieMode; /* 1152 */
		_settings->SendPreconnectionPdu = settings->SendPreconnectionPdu; /* 1156 */
		_settings->IgnoreCertificate = settings->IgnoreCertificate; /* 1408 */
		_settings->ExternalCertificateManagement = settings->ExternalCertificateManagement; /* 1415 */
		_settings->Workarea = settings->Workarea; /* 1536 */
		_settings->Fullscreen = settings->Fullscreen; /* 1537 */
		_settings->GrabKeyboard = settings->GrabKeyboard; /* 1539 */
		_settings->Decorations = settings->Decorations; /* 1540 */
		_settings->MouseMotion = settings->MouseMotion; /* 1541 */
		_settings->AsyncInput = settings->AsyncInput; /* 1544 */
		_settings->AsyncUpdate = settings->AsyncUpdate; /* 1545 */
		_settings->AsyncChannels = settings->AsyncChannels; /* 1546 */
		_settings->AsyncTransport = settings->AsyncTransport; /* 1547 */
		_settings->ToggleFullscreen = settings->ToggleFullscreen; /* 1548 */
		_settings->EmbeddedWindow = settings->EmbeddedWindow; /* 1550 */
		_settings->SmartSizing = settings->SmartSizing; /* 1551 */
		_settings->SoftwareGdi = settings->SoftwareGdi; /* 1601 */
		_settings->LocalConnection = settings->LocalConnection; /* 1602 */
		_settings->AuthenticationOnly = settings->AuthenticationOnly; /* 1603 */
		_settings->CredentialsFromStdin = settings->CredentialsFromStdin; /* 1604 */
		_settings->DumpRemoteFx = settings->DumpRemoteFx; /* 1856 */
		_settings->PlayRemoteFx = settings->PlayRemoteFx; /* 1857 */
		_settings->GatewayUseSameCredentials = settings->GatewayUseSameCredentials; /* 1991 */
		_settings->GatewayEnabled = settings->GatewayEnabled; /* 1992 */
		_settings->RemoteApplicationMode = settings->RemoteApplicationMode; /* 2112 */
		_settings->DisableRemoteAppCapsCheck = settings->DisableRemoteAppCapsCheck; /* 2121 */
		_settings->RemoteAppLanguageBarSupported = settings->RemoteAppLanguageBarSupported; /* 2124 */
		_settings->RefreshRect = settings->RefreshRect; /* 2306 */
		_settings->SuppressOutput = settings->SuppressOutput; /* 2307 */
		_settings->FastPathOutput = settings->FastPathOutput; /* 2308 */
		_settings->SaltedChecksum = settings->SaltedChecksum; /* 2309 */
		_settings->LongCredentialsSupported = settings->LongCredentialsSupported; /* 2310 */
		_settings->NoBitmapCompressionHeader = settings->NoBitmapCompressionHeader; /* 2311 */
		_settings->BitmapCompressionDisabled = settings->BitmapCompressionDisabled; /* 2312 */
		_settings->DesktopResize = settings->DesktopResize; /* 2368 */
		_settings->DrawAllowDynamicColorFidelity = settings->DrawAllowDynamicColorFidelity; /* 2369 */
		_settings->DrawAllowColorSubsampling = settings->DrawAllowColorSubsampling; /* 2370 */
		_settings->DrawAllowSkipAlpha = settings->DrawAllowSkipAlpha; /* 2371 */
		_settings->BitmapCacheV3Enabled = settings->BitmapCacheV3Enabled; /* 2433 */
		_settings->AltSecFrameMarkerSupport = settings->AltSecFrameMarkerSupport; /* 2434 */
		_settings->BitmapCacheEnabled = settings->BitmapCacheEnabled; /* 2497 */
		_settings->AllowCacheWaitingList = settings->AllowCacheWaitingList; /* 2499 */
		_settings->BitmapCachePersistEnabled = settings->BitmapCachePersistEnabled; /* 2500 */
		_settings->ColorPointerFlag = settings->ColorPointerFlag; /* 2560 */
		_settings->UnicodeInput = settings->UnicodeInput; /* 2629 */
		_settings->FastPathInput = settings->FastPathInput; /* 2630 */
		_settings->MultiTouchInput = settings->MultiTouchInput; /* 2631 */
		_settings->MultiTouchGestures = settings->MultiTouchGestures; /* 2632 */
		_settings->SoundBeepsEnabled = settings->SoundBeepsEnabled; /* 2944 */
		_settings->SurfaceCommandsEnabled = settings->SurfaceCommandsEnabled; /* 3520 */
		_settings->FrameMarkerCommandEnabled = settings->FrameMarkerCommandEnabled; /* 3521 */
		_settings->SurfaceFrameMarkerEnabled = settings->SurfaceFrameMarkerEnabled; /* 3522 */
		_settings->RemoteFxOnly = settings->RemoteFxOnly; /* 3648 */
		_settings->RemoteFxCodec = settings->RemoteFxCodec; /* 3649 */
		_settings->RemoteFxImageCodec = settings->RemoteFxImageCodec; /* 3652 */
		_settings->NSCodec = settings->NSCodec; /* 3712 */
		_settings->JpegCodec = settings->JpegCodec; /* 3776 */
		_settings->DrawNineGridEnabled = settings->DrawNineGridEnabled; /* 3968 */
		_settings->DrawGdiPlusEnabled = settings->DrawGdiPlusEnabled; /* 4032 */
		_settings->DrawGdiPlusCacheEnabled = settings->DrawGdiPlusCacheEnabled; /* 4033 */
		_settings->DeviceRedirection = settings->DeviceRedirection; /* 4160 */
		_settings->RedirectDrives = settings->RedirectDrives; /* 4288 */
		_settings->RedirectHomeDrive = settings->RedirectHomeDrive; /* 4289 */
		_settings->RedirectSmartCards = settings->RedirectSmartCards; /* 4416 */
		_settings->RedirectPrinters = settings->RedirectPrinters; /* 4544 */
		_settings->RedirectSerialPorts = settings->RedirectSerialPorts; /* 4672 */
		_settings->RedirectParallelPorts = settings->RedirectParallelPorts; /* 4673 */
		_settings->RedirectClipboard = settings->RedirectClipboard; /* 4800 */

		/**
		  * Manual Code
		  */

		_settings->ChannelCount = settings->ChannelCount;
		_settings->ChannelDefArraySize = settings->ChannelDefArraySize;
		_settings->ChannelDefArray = (rdpChannel*) malloc(sizeof(rdpChannel) * settings->ChannelDefArraySize);
		CopyMemory(_settings->ChannelDefArray, settings->ChannelDefArray, sizeof(rdpChannel) * settings->ChannelDefArraySize);

		_settings->MonitorCount = settings->MonitorCount;
		_settings->MonitorDefArraySize = settings->MonitorDefArraySize;
		_settings->MonitorDefArray = (rdpMonitor*) malloc(sizeof(rdpMonitor) * settings->MonitorDefArraySize);
		CopyMemory(_settings->MonitorDefArray, settings->MonitorDefArray, sizeof(rdpMonitor) * settings->MonitorDefArraySize);

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
		CopyMemory(_settings->ClientTimeZone, _settings->ClientTimeZone, sizeof(TIME_ZONE_INFO));

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
		_settings->StaticChannelArray = (ADDIN_ARGV**)
				malloc(sizeof(ADDIN_ARGV*) * _settings->StaticChannelArraySize);
		ZeroMemory(_settings->StaticChannelArray, sizeof(ADDIN_ARGV*) * _settings->StaticChannelArraySize);

		for (index = 0; index < _settings->StaticChannelCount; index++)
		{
			_settings->StaticChannelArray[index] = freerdp_static_channel_clone(settings->StaticChannelArray[index]);
		}

		_settings->DynamicChannelCount = settings->DynamicChannelCount;
		_settings->DynamicChannelArraySize = settings->DynamicChannelArraySize;
		_settings->DynamicChannelArray = (ADDIN_ARGV**)
				malloc(sizeof(ADDIN_ARGV*) * _settings->DynamicChannelArraySize);
		ZeroMemory(_settings->DynamicChannelArray, sizeof(ADDIN_ARGV*) * _settings->DynamicChannelArraySize);

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
		free(settings->ClientAddress);
		free(settings->ClientDir);
		free(settings->CertificateFile);
		free(settings->PrivateKeyFile);
		free(settings->ReceivedCapabilities);
		free(settings->OrderSupport);
		free(settings->ClientHostname);
		free(settings->ClientProductId);
		free(settings->ServerRandom);
		if (settings->ClientRandom) free(settings->ClientRandom);
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

