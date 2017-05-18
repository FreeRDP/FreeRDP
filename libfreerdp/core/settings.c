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

#include <ctype.h>

#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/sysinfo.h>
#include <winpr/registry.h>

#include <freerdp/settings.h>
#include <freerdp/build-config.h>
#include <ctype.h>


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

#define SERVER_KEY "Software\\" FREERDP_VENDOR_STRING "\\" \
	FREERDP_PRODUCT_STRING "\\Server"
#define CLIENT_KEY "Software\\" FREERDP_VENDOR_STRING "\\" \
	FREERDP_PRODUCT_STRING "\\Client"
#define BITMAP_CACHE_KEY CLIENT_KEY "\\BitmapCacheV2"
#define GLYPH_CACHE_KEY CLIENT_KEY "\\GlyphCache"
#define POINTER_CACHE_KEY CLIENT_KEY "\\PointerCache"

void settings_client_load_hkey_local_machine(rdpSettings* settings)
{
	HKEY hKey;
	LONG status;
	DWORD dwType;
	DWORD dwSize;
	DWORD dwValue;
	status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, CLIENT_KEY, 0,
	                       KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		REG_QUERY_DWORD_VALUE(hKey, _T("DesktopWidth"), dwType, dwValue, dwSize,
		                      settings->DesktopWidth);
		REG_QUERY_DWORD_VALUE(hKey, _T("DesktopHeight"), dwType, dwValue, dwSize,
		                      settings->DesktopHeight);
		REG_QUERY_BOOL_VALUE(hKey, _T("Fullscreen"), dwType, dwValue, dwSize,
		                     settings->Fullscreen);
		REG_QUERY_DWORD_VALUE(hKey, _T("ColorDepth"), dwType, dwValue, dwSize,
		                      settings->ColorDepth);
		REG_QUERY_DWORD_VALUE(hKey, _T("KeyboardType"), dwType, dwValue, dwSize,
		                      settings->KeyboardType);
		REG_QUERY_DWORD_VALUE(hKey, _T("KeyboardSubType"), dwType, dwValue, dwSize,
		                      settings->KeyboardSubType);
		REG_QUERY_DWORD_VALUE(hKey, _T("KeyboardFunctionKeys"), dwType, dwValue, dwSize,
		                      settings->KeyboardFunctionKey);
		REG_QUERY_DWORD_VALUE(hKey, _T("KeyboardLayout"), dwType, dwValue, dwSize,
		                      settings->KeyboardLayout);
		REG_QUERY_BOOL_VALUE(hKey, _T("ExtSecurity"), dwType, dwValue, dwSize,
		                     settings->ExtSecurity);
		REG_QUERY_BOOL_VALUE(hKey, _T("NlaSecurity"), dwType, dwValue, dwSize,
		                     settings->NlaSecurity);
		REG_QUERY_BOOL_VALUE(hKey, _T("TlsSecurity"), dwType, dwValue, dwSize,
		                     settings->TlsSecurity);
		REG_QUERY_BOOL_VALUE(hKey, _T("RdpSecurity"), dwType, dwValue, dwSize,
		                     settings->RdpSecurity);
		REG_QUERY_BOOL_VALUE(hKey, _T("MstscCookieMode"), dwType, dwValue, dwSize,
		                     settings->MstscCookieMode);
		REG_QUERY_DWORD_VALUE(hKey, _T("CookieMaxLength"), dwType, dwValue, dwSize,
		                      settings->CookieMaxLength);
		REG_QUERY_BOOL_VALUE(hKey, _T("BitmapCache"), dwType, dwValue, dwSize,
		                     settings->BitmapCacheEnabled);
		REG_QUERY_BOOL_VALUE(hKey, _T("OffscreenBitmapCache"), dwType, dwValue, dwSize,
		                     settings->OffscreenSupportLevel);
		REG_QUERY_DWORD_VALUE(hKey, _T("OffscreenBitmapCacheSize"), dwType, dwValue,
		                      dwSize, settings->OffscreenCacheSize);
		REG_QUERY_DWORD_VALUE(hKey, _T("OffscreenBitmapCacheEntries"), dwType, dwValue,
		                      dwSize, settings->OffscreenCacheEntries);
		RegCloseKey(hKey);
	}

	status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, BITMAP_CACHE_KEY, 0,
	                       KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		REG_QUERY_DWORD_VALUE(hKey, _T("NumCells"), dwType, dwValue, dwSize,
		                      settings->BitmapCacheV2NumCells);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cell0NumEntries"), dwType, dwValue, dwSize,
		                      settings->BitmapCacheV2CellInfo[0].numEntries);
		REG_QUERY_BOOL_VALUE(hKey, _T("Cell0Persistent"), dwType, dwValue, dwSize,
		                     settings->BitmapCacheV2CellInfo[0].persistent);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cell1NumEntries"), dwType, dwValue, dwSize,
		                      settings->BitmapCacheV2CellInfo[1].numEntries);
		REG_QUERY_BOOL_VALUE(hKey, _T("Cell1Persistent"), dwType, dwValue, dwSize,
		                     settings->BitmapCacheV2CellInfo[1].persistent);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cell2NumEntries"), dwType, dwValue, dwSize,
		                      settings->BitmapCacheV2CellInfo[2].numEntries);
		REG_QUERY_BOOL_VALUE(hKey, _T("Cell2Persistent"), dwType, dwValue, dwSize,
		                     settings->BitmapCacheV2CellInfo[2].persistent);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cell3NumEntries"), dwType, dwValue, dwSize,
		                      settings->BitmapCacheV2CellInfo[3].numEntries);
		REG_QUERY_BOOL_VALUE(hKey, _T("Cell3Persistent"), dwType, dwValue, dwSize,
		                     settings->BitmapCacheV2CellInfo[3].persistent);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cell4NumEntries"), dwType, dwValue, dwSize,
		                      settings->BitmapCacheV2CellInfo[4].numEntries);
		REG_QUERY_BOOL_VALUE(hKey, _T("Cell4Persistent"), dwType, dwValue, dwSize,
		                     settings->BitmapCacheV2CellInfo[4].persistent);
		REG_QUERY_BOOL_VALUE(hKey, _T("AllowCacheWaitingList"), dwType, dwValue, dwSize,
		                     settings->AllowCacheWaitingList);
		RegCloseKey(hKey);
	}

	status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, GLYPH_CACHE_KEY,
	                       0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		REG_QUERY_DWORD_VALUE(hKey, _T("SupportLevel"), dwType, dwValue, dwSize,
		                      settings->GlyphSupportLevel);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache0NumEntries"), dwType, dwValue, dwSize,
		                      settings->GlyphCache[0].cacheEntries);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache0MaxCellSize"), dwType, dwValue, dwSize,
		                      settings->GlyphCache[0].cacheMaximumCellSize);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache1NumEntries"), dwType, dwValue, dwSize,
		                      settings->GlyphCache[1].cacheEntries);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache1MaxCellSize"), dwType, dwValue, dwSize,
		                      settings->GlyphCache[1].cacheMaximumCellSize);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache2NumEntries"), dwType, dwValue, dwSize,
		                      settings->GlyphCache[2].cacheEntries);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache2MaxCellSize"), dwType, dwValue, dwSize,
		                      settings->GlyphCache[2].cacheMaximumCellSize);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache3NumEntries"), dwType, dwValue, dwSize,
		                      settings->GlyphCache[3].cacheEntries);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache3MaxCellSize"), dwType, dwValue, dwSize,
		                      settings->GlyphCache[3].cacheMaximumCellSize);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache4NumEntries"), dwType, dwValue, dwSize,
		                      settings->GlyphCache[4].cacheEntries);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache4MaxCellSize"), dwType, dwValue, dwSize,
		                      settings->GlyphCache[4].cacheMaximumCellSize);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache5NumEntries"), dwType, dwValue, dwSize,
		                      settings->GlyphCache[5].cacheEntries);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache5MaxCellSize"), dwType, dwValue, dwSize,
		                      settings->GlyphCache[5].cacheMaximumCellSize);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache6NumEntries"), dwType, dwValue, dwSize,
		                      settings->GlyphCache[6].cacheEntries);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache6MaxCellSize"), dwType, dwValue, dwSize,
		                      settings->GlyphCache[6].cacheMaximumCellSize);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache7NumEntries"), dwType, dwValue, dwSize,
		                      settings->GlyphCache[7].cacheEntries);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache7MaxCellSize"), dwType, dwValue, dwSize,
		                      settings->GlyphCache[7].cacheMaximumCellSize);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache8NumEntries"), dwType, dwValue, dwSize,
		                      settings->GlyphCache[8].cacheEntries);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache8MaxCellSize"), dwType, dwValue, dwSize,
		                      settings->GlyphCache[8].cacheMaximumCellSize);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache9NumEntries"), dwType, dwValue, dwSize,
		                      settings->GlyphCache[9].cacheEntries);
		REG_QUERY_DWORD_VALUE(hKey, _T("Cache9MaxCellSize"), dwType, dwValue, dwSize,
		                      settings->GlyphCache[9].cacheMaximumCellSize);
		REG_QUERY_DWORD_VALUE(hKey, _T("FragCacheNumEntries"), dwType, dwValue, dwSize,
		                      settings->FragCache->cacheEntries);
		REG_QUERY_DWORD_VALUE(hKey, _T("FragCacheMaxCellSize"), dwType, dwValue, dwSize,
		                      settings->FragCache->cacheMaximumCellSize);
		RegCloseKey(hKey);
	}

	status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, POINTER_CACHE_KEY,
	                       0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		REG_QUERY_BOOL_VALUE(hKey, _T("LargePointer"), dwType, dwValue, dwSize,
		                     settings->LargePointerFlag);
		REG_QUERY_BOOL_VALUE(hKey, _T("ColorPointer"), dwType, dwValue, dwSize,
		                     settings->ColorPointerFlag);
		REG_QUERY_DWORD_VALUE(hKey, _T("PointerCacheSize"), dwType, dwValue, dwSize,
		                      settings->PointerCacheSize);
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
	status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, SERVER_KEY,
	                       0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status != ERROR_SUCCESS)
		return;

	REG_QUERY_BOOL_VALUE(hKey, _T("ExtSecurity"), dwType, dwValue, dwSize,
	                     settings->ExtSecurity);
	REG_QUERY_BOOL_VALUE(hKey, _T("NlaSecurity"), dwType, dwValue, dwSize,
	                     settings->NlaSecurity);
	REG_QUERY_BOOL_VALUE(hKey, _T("TlsSecurity"), dwType, dwValue, dwSize,
	                     settings->TlsSecurity);
	REG_QUERY_BOOL_VALUE(hKey, _T("RdpSecurity"), dwType, dwValue, dwSize,
	                     settings->RdpSecurity);
	RegCloseKey(hKey);
}

void settings_load_hkey_local_machine(rdpSettings* settings)
{
	if (settings->ServerMode)
		settings_server_load_hkey_local_machine(settings);
	else
		settings_client_load_hkey_local_machine(settings);
}

BOOL settings_get_computer_name(rdpSettings* settings)
{
	DWORD nSize = 0;
	CHAR* computerName;

	if (GetComputerNameExA(ComputerNameNetBIOS, NULL, &nSize) || (GetLastError() != ERROR_MORE_DATA) ||
	    (nSize < 2))
		return FALSE;

	computerName = calloc(nSize, sizeof(CHAR));

	if (!computerName)
		return FALSE;

	if (!GetComputerNameExA(ComputerNameNetBIOS, computerName, &nSize))
	{
		free(computerName);
		return FALSE;
	}

	if (nSize > MAX_COMPUTERNAME_LENGTH)
		computerName[MAX_COMPUTERNAME_LENGTH] = '\0';

	settings->ComputerName = computerName;

	if (!settings->ComputerName)
		return FALSE;

	return TRUE;
}

rdpSettings* freerdp_settings_new(DWORD flags)
{
	char* base;
	rdpSettings* settings;
	settings = (rdpSettings*) calloc(1, sizeof(rdpSettings));

	if (!settings)
		return NULL;

	settings->ServerMode = (flags & FREERDP_SETTINGS_SERVER_MODE) ? TRUE : FALSE;
	settings->WaitForOutputBufferFlush = TRUE;
	settings->MaxTimeInCheckLoop = 100;
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
	settings->UnmapButtons = FALSE;
	settings->PerformanceFlags = PERF_FLAG_NONE;
	settings->AllowFontSmoothing = FALSE;
	settings->AllowDesktopComposition = FALSE;
	settings->DisableWallpaper = FALSE;
	settings->DisableFullWindowDrag = TRUE;
	settings->DisableMenuAnims = TRUE;
	settings->DisableThemes = FALSE;
	settings->ConnectionType = CONNECTION_TYPE_LAN;
	settings->EncryptionMethods = ENCRYPTION_METHOD_NONE;
	settings->EncryptionLevel = ENCRYPTION_LEVEL_NONE;
	settings->CompressionEnabled = TRUE;
	settings->LogonNotify = TRUE;

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
	settings->ChannelDefArray = (CHANNEL_DEF*) calloc(settings->ChannelDefArraySize,
	                            sizeof(CHANNEL_DEF));

	if (!settings->ChannelDefArray)
		goto out_fail;

	settings->SupportMonitorLayoutPdu = FALSE;
	settings->MonitorCount = 0;
	settings->MonitorDefArraySize = 32;
	settings->MonitorDefArray = (rdpMonitor*) calloc(settings->MonitorDefArraySize,
	                            sizeof(rdpMonitor));

	if (!settings->MonitorDefArray)
		goto out_fail;

	settings->MonitorLocalShiftX = 0;
	settings->MonitorLocalShiftY = 0;
	settings->MonitorIds = (UINT32*) calloc(16, sizeof(UINT32));

	if (!settings->MonitorIds)
		goto out_fail;

	if (!settings_get_computer_name(settings))
		goto out_fail;

	settings->ReceivedCapabilities = calloc(1, 32);

	if (!settings->ReceivedCapabilities)
		goto out_fail;

	settings->OrderSupport = calloc(1, 32);

	if (!settings->OrderSupport)
		goto out_fail;

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
	settings->ClientProductId = calloc(1, 32);

	if (!settings->ClientProductId)
		goto out_fail;

	settings->ClientHostname = calloc(1, 32);

	if (!settings->ClientHostname)
		goto out_fail;

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
	settings->BitmapCacheV2CellInfo = (BITMAP_CACHE_V2_CELL_INFO*) malloc(sizeof(
	                                      BITMAP_CACHE_V2_CELL_INFO) * 6);

	if (!settings->BitmapCacheV2CellInfo)
		goto out_fail;

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
	settings->GlyphSupportLevel = GLYPH_SUPPORT_NONE;
	settings->GlyphCache = malloc(sizeof(GLYPH_CACHE_DEFINITION) * 10);

	if (!settings->GlyphCache)
		goto out_fail;

	settings->FragCache = malloc(sizeof(GLYPH_CACHE_DEFINITION));

	if (!settings->FragCache)
		goto out_fail;

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

	if (!settings->ClientDir)
		goto out_fail;

	settings->RemoteAppNumIconCaches = 3;
	settings->RemoteAppNumIconCacheEntries = 12;
	settings->VirtualChannelChunkSize = CHANNEL_CHUNK_LENGTH;
	settings->MultifragMaxRequestSize = (flags & FREERDP_SETTINGS_SERVER_MODE) ?
	                                    0 : 0xFFFF;
	settings->GatewayUseSameCredentials = FALSE;
	settings->GatewayBypassLocal = FALSE;
	settings->GatewayRpcTransport = TRUE;
	settings->GatewayHttpTransport = TRUE;
	settings->GatewayUdpTransport = TRUE;
	settings->FastPathInput = TRUE;
	settings->FastPathOutput = TRUE;
	settings->LongCredentialsSupported = TRUE;
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
	settings->GfxAVC444 = FALSE;
	settings->GfxSendQoeAck = FALSE;
	settings->ClientAutoReconnectCookie = (ARC_CS_PRIVATE_PACKET*) calloc(1,
	                                      sizeof(ARC_CS_PRIVATE_PACKET));

	if (!settings->ClientAutoReconnectCookie)
		goto out_fail;

	settings->ServerAutoReconnectCookie = (ARC_SC_PRIVATE_PACKET*) calloc(1,
	                                      sizeof(ARC_SC_PRIVATE_PACKET));

	if (!settings->ServerAutoReconnectCookie)
		goto out_fail;

	settings->ClientTimeZone = (LPTIME_ZONE_INFORMATION) calloc(1,
	                           sizeof(TIME_ZONE_INFORMATION));

	if (!settings->ClientTimeZone)
		goto out_fail;

	settings->DeviceArraySize = 16;
	settings->DeviceArray = (RDPDR_DEVICE**) calloc(1,
	                        sizeof(RDPDR_DEVICE*) * settings->DeviceArraySize);

	if (!settings->DeviceArray)
		goto out_fail;

	settings->StaticChannelArraySize = 16;
	settings->StaticChannelArray = (ADDIN_ARGV**)
	                               calloc(1, sizeof(ADDIN_ARGV*) * settings->StaticChannelArraySize);

	if (!settings->StaticChannelArray)
		goto out_fail;

	settings->DynamicChannelArraySize = 16;
	settings->DynamicChannelArray = (ADDIN_ARGV**)
	                                calloc(1, sizeof(ADDIN_ARGV*) * settings->DynamicChannelArraySize);

	if (!settings->DynamicChannelArray)
		goto out_fail;

	if (!settings->ServerMode)
	{
		/* these values are used only by the client part */
		settings->HomePath = GetKnownPath(KNOWN_PATH_HOME);

		if (!settings->HomePath)
			goto out_fail;

		/* For default FreeRDP continue using same config directory
		 * as in old releases.
		 * Custom builds use <Vendor>/<Product> as config folder. */
		if (_stricmp(FREERDP_VENDOR_STRING, FREERDP_PRODUCT_STRING))
		{
			base = GetKnownSubPath(KNOWN_PATH_XDG_CONFIG_HOME,
			                       FREERDP_VENDOR_STRING);

			if (base)
			{
				settings->ConfigPath = GetCombinedPath(
				                           base,
				                           FREERDP_PRODUCT_STRING);
			}

			free(base);
		}
		else
		{
			int i;
			char product[sizeof(FREERDP_PRODUCT_STRING)];
			memset(product, 0, sizeof(product));

			for (i = 0; i < sizeof(product); i++)
				product[i] = tolower(FREERDP_PRODUCT_STRING[i]);

			settings->ConfigPath = GetKnownSubPath(
			                           KNOWN_PATH_XDG_CONFIG_HOME,
			                           product);
		}

		if (!settings->ConfigPath)
			goto out_fail;
	}

	settings_load_hkey_local_machine(settings);
	settings->SettingsModified = (BYTE*) calloc(1, sizeof(rdpSettings) / 8);

	if (!settings->SettingsModified)
		goto out_fail;

	settings->ActionScript = _strdup("~/.config/freerdp/action.sh");
	return settings;
out_fail:
	free(settings->HomePath);
	free(settings->ConfigPath);
	free(settings->DynamicChannelArray);
	free(settings->StaticChannelArray);
	free(settings->DeviceArray);
	free(settings->ClientTimeZone);
	free(settings->ServerAutoReconnectCookie);
	free(settings->ClientAutoReconnectCookie);
	free(settings->ClientDir);
	free(settings->FragCache);
	free(settings->GlyphCache);
	free(settings->BitmapCacheV2CellInfo);
	free(settings->ClientProductId);
	free(settings->ClientHostname);
	free(settings->OrderSupport);
	free(settings->ReceivedCapabilities);
	free(settings->ComputerName);
	free(settings->MonitorIds);
	free(settings->MonitorDefArray);
	free(settings->ChannelDefArray);
	free(settings);
	return NULL;
}

rdpSettings* freerdp_settings_clone(rdpSettings* settings)
{
	UINT32 index;
	rdpSettings* _settings;
	_settings = (rdpSettings*) calloc(1, sizeof(rdpSettings));

	if (_settings)
	{
		CopyMemory(_settings, settings, sizeof(rdpSettings));
		/* char* values */
#define CHECKED_STRDUP(name) if (settings->name && !(_settings->name = _strdup(settings->name))) goto out_fail
		CHECKED_STRDUP(ServerHostname);  /* 20 */
		CHECKED_STRDUP(Username); /* 21 */
		CHECKED_STRDUP(Password); /* 22 */
		CHECKED_STRDUP(Domain); /* 23 */
		CHECKED_STRDUP(PasswordHash); /* 24 */
		_settings->ClientHostname = NULL; /* 134 */
		_settings->ClientProductId = NULL; /* 135 */
		CHECKED_STRDUP(AlternateShell); /* 640 */
		CHECKED_STRDUP(ShellWorkingDirectory); /* 641 */
		CHECKED_STRDUP(ClientAddress); /* 769 */
		CHECKED_STRDUP(ClientDir); /* 770 */
		CHECKED_STRDUP(DynamicDSTTimeZoneKeyName); /* 897 */
		CHECKED_STRDUP(RemoteAssistanceSessionId); /* 1025 */
		CHECKED_STRDUP(RemoteAssistancePassStub); /* 1026 */
		CHECKED_STRDUP(RemoteAssistancePassword); /* 1027 */
		CHECKED_STRDUP(RemoteAssistanceRCTicket); /* 1028 */
		CHECKED_STRDUP(AuthenticationServiceClass); /* 1098 */
		CHECKED_STRDUP(AllowedTlsCiphers); /* 1101 */
		CHECKED_STRDUP(NtlmSamFile); /* 1103 */
		CHECKED_STRDUP(PreconnectionBlob); /* 1155 */
		CHECKED_STRDUP(KerberosKdc); /* 1344 */
		CHECKED_STRDUP(KerberosRealm); /* 1345 */
		CHECKED_STRDUP(CertificateName); /* 1409 */
		CHECKED_STRDUP(CertificateFile); /* 1410 */
		CHECKED_STRDUP(PrivateKeyFile); /* 1411 */
		CHECKED_STRDUP(RdpKeyFile); /* 1412 */
		CHECKED_STRDUP(CertificateContent); /* 1416 */
		CHECKED_STRDUP(PrivateKeyContent); /* 1417 */
		CHECKED_STRDUP(RdpKeyContent); /* 1418 */
		CHECKED_STRDUP(WindowTitle); /* 1542 */
		CHECKED_STRDUP(WmClass); /* 1549 */
		CHECKED_STRDUP(ComputerName); /* 1664 */
		CHECKED_STRDUP(ConnectionFile); /* 1728 */
		CHECKED_STRDUP(AssistanceFile); /* 1729 */
		CHECKED_STRDUP(HomePath); /* 1792 */
		CHECKED_STRDUP(ConfigPath); /* 1793 */
		CHECKED_STRDUP(CurrentPath); /* 1794 */
		CHECKED_STRDUP(DumpRemoteFxFile); /* 1858 */
		CHECKED_STRDUP(PlayRemoteFxFile); /* 1859 */
		CHECKED_STRDUP(GatewayHostname); /* 1986 */
		CHECKED_STRDUP(GatewayUsername); /* 1987 */
		CHECKED_STRDUP(GatewayPassword); /* 1988 */
		CHECKED_STRDUP(GatewayDomain); /* 1989 */
		CHECKED_STRDUP(ProxyHostname); /* 2016 */
		CHECKED_STRDUP(RemoteApplicationName); /* 2113 */
		CHECKED_STRDUP(RemoteApplicationIcon); /* 2114 */
		CHECKED_STRDUP(RemoteApplicationProgram); /* 2115 */
		CHECKED_STRDUP(RemoteApplicationFile); /* 2116 */
		CHECKED_STRDUP(RemoteApplicationGuid); /* 2117 */
		CHECKED_STRDUP(RemoteApplicationCmdLine); /* 2118 */
		CHECKED_STRDUP(ImeFileName); /* 2628 */
		CHECKED_STRDUP(DrivesToRedirect); /* 4290 */
		CHECKED_STRDUP(ActionScript);
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
		_settings->TargetNetPorts = NULL;

		if (settings->LoadBalanceInfo && settings->LoadBalanceInfoLength)
		{
			_settings->LoadBalanceInfo = (BYTE*) calloc(1,
			                             settings->LoadBalanceInfoLength + 2);

			if (!_settings->LoadBalanceInfo)
				goto out_fail;

			CopyMemory(_settings->LoadBalanceInfo, settings->LoadBalanceInfo,
			           settings->LoadBalanceInfoLength);
			_settings->LoadBalanceInfoLength = settings->LoadBalanceInfoLength;
		}

		if (_settings->ServerRandomLength)
		{
			_settings->ServerRandom = (BYTE*) malloc(_settings->ServerRandomLength);

			if (!_settings->ServerRandom)
				goto out_fail;

			CopyMemory(_settings->ServerRandom, settings->ServerRandom,
			           _settings->ServerRandomLength);
			_settings->ServerRandomLength = settings->ServerRandomLength;
		}

		if (_settings->ClientRandomLength)
		{
			_settings->ClientRandom = (BYTE*) malloc(_settings->ClientRandomLength);

			if (!_settings->ClientRandom)
				goto out_fail;

			CopyMemory(_settings->ClientRandom, settings->ClientRandom,
			           _settings->ClientRandomLength);
			_settings->ClientRandomLength = settings->ClientRandomLength;
		}

		if (settings->RdpServerCertificate)
		{
			_settings->RdpServerCertificate = certificate_clone(
			                                      settings->RdpServerCertificate);

			if (!_settings->RdpServerCertificate)
				goto out_fail;
		}

		_settings->ChannelCount = settings->ChannelCount;
		_settings->ChannelDefArraySize = settings->ChannelDefArraySize;

		if (_settings->ChannelDefArraySize > 0)
		{
			_settings->ChannelDefArray = (CHANNEL_DEF*) malloc(sizeof(
			                                 CHANNEL_DEF) * settings->ChannelDefArraySize);

			if (!_settings->ChannelDefArray)
				goto out_fail;

			CopyMemory(_settings->ChannelDefArray, settings->ChannelDefArray,
			           sizeof(CHANNEL_DEF) * settings->ChannelDefArraySize);
		}
		else
			_settings->ChannelDefArray = NULL;

		_settings->MonitorCount = settings->MonitorCount;
		_settings->MonitorDefArraySize = settings->MonitorDefArraySize;

		if (_settings->MonitorDefArraySize > 0)
		{
			_settings->MonitorDefArray = (rdpMonitor*) malloc(sizeof(
			                                 rdpMonitor) * settings->MonitorDefArraySize);

			if (!_settings->MonitorDefArray)
				goto out_fail;

			CopyMemory(_settings->MonitorDefArray, settings->MonitorDefArray,
			           sizeof(rdpMonitor) * settings->MonitorDefArraySize);
		}
		else
			_settings->MonitorDefArray = NULL;

		_settings->MonitorIds = (UINT32*) calloc(16, sizeof(UINT32));

		if (!_settings->MonitorIds)
			goto out_fail;

		CopyMemory(_settings->MonitorIds, settings->MonitorIds, 16 * sizeof(UINT32));
		_settings->ReceivedCapabilities = malloc(32);

		if (!_settings->ReceivedCapabilities)
			goto out_fail;

		_settings->OrderSupport = malloc(32);

		if (!_settings->OrderSupport)
			goto out_fail;

		if (!_settings->ReceivedCapabilities || !_settings->OrderSupport)
			goto out_fail;

		CopyMemory(_settings->ReceivedCapabilities, settings->ReceivedCapabilities, 32);
		CopyMemory(_settings->OrderSupport, settings->OrderSupport, 32);
		_settings->ClientHostname = _strdup(settings->ClientHostname);

		if (!_settings->ClientHostname)
			goto out_fail;

		_settings->ClientProductId = _strdup(settings->ClientProductId);

		if (!_settings->ClientProductId)
			goto out_fail;

		_settings->BitmapCacheV2CellInfo = (BITMAP_CACHE_V2_CELL_INFO*) malloc(sizeof(
		                                       BITMAP_CACHE_V2_CELL_INFO) * 6);

		if (!_settings->BitmapCacheV2CellInfo)
			goto out_fail;

		CopyMemory(_settings->BitmapCacheV2CellInfo, settings->BitmapCacheV2CellInfo,
		           sizeof(BITMAP_CACHE_V2_CELL_INFO) * 6);
		_settings->GlyphCache = malloc(sizeof(GLYPH_CACHE_DEFINITION) * 10);

		if (!_settings->GlyphCache)
			goto out_fail;

		_settings->FragCache = malloc(sizeof(GLYPH_CACHE_DEFINITION));

		if (!_settings->FragCache)
			goto out_fail;

		CopyMemory(_settings->GlyphCache, settings->GlyphCache,
		           sizeof(GLYPH_CACHE_DEFINITION) * 10);
		CopyMemory(_settings->FragCache, settings->FragCache,
		           sizeof(GLYPH_CACHE_DEFINITION));
		_settings->ClientAutoReconnectCookie = (ARC_CS_PRIVATE_PACKET*) malloc(sizeof(
		        ARC_CS_PRIVATE_PACKET));

		if (!_settings->ClientAutoReconnectCookie)
			goto out_fail;

		_settings->ServerAutoReconnectCookie = (ARC_SC_PRIVATE_PACKET*) malloc(sizeof(
		        ARC_SC_PRIVATE_PACKET));

		if (!_settings->ServerAutoReconnectCookie)
			goto out_fail;

		CopyMemory(_settings->ClientAutoReconnectCookie,
		           settings->ClientAutoReconnectCookie, sizeof(ARC_CS_PRIVATE_PACKET));
		CopyMemory(_settings->ServerAutoReconnectCookie,
		           settings->ServerAutoReconnectCookie, sizeof(ARC_SC_PRIVATE_PACKET));
		_settings->ClientTimeZone = (LPTIME_ZONE_INFORMATION) malloc(sizeof(
		                                TIME_ZONE_INFORMATION));

		if (!_settings->ClientTimeZone)
			goto out_fail;

		CopyMemory(_settings->ClientTimeZone, settings->ClientTimeZone,
		           sizeof(TIME_ZONE_INFORMATION));
		_settings->TargetNetAddressCount = settings->TargetNetAddressCount;

		if (settings->TargetNetAddressCount > 0)
		{
			_settings->TargetNetAddresses = (char**) calloc(settings->TargetNetAddressCount,
			                                sizeof(char*));

			if (!_settings->TargetNetAddresses)
			{
				_settings->TargetNetAddressCount = 0;
				goto out_fail;
			}

			for (index = 0; index < settings->TargetNetAddressCount; index++)
			{
				_settings->TargetNetAddresses[index] = _strdup(
				        settings->TargetNetAddresses[index]);

				if (!_settings->TargetNetAddresses[index])
				{
					while (index)
						free(_settings->TargetNetAddresses[--index]);

					free(_settings->TargetNetAddresses);
					_settings->TargetNetAddresses = NULL;
					_settings->TargetNetAddressCount = 0;
					goto out_fail;
				}
			}

			if (settings->TargetNetPorts)
			{
				_settings->TargetNetPorts = (UINT32*) calloc(settings->TargetNetAddressCount,
				                            sizeof(UINT32));

				if (!_settings->TargetNetPorts)
					goto out_fail;

				for (index = 0; index < settings->TargetNetAddressCount; index++)
					_settings->TargetNetPorts[index] = settings->TargetNetPorts[index];
			}
		}

		_settings->DeviceCount = settings->DeviceCount;
		_settings->DeviceArraySize = settings->DeviceArraySize;
		_settings->DeviceArray = (RDPDR_DEVICE**) calloc(_settings->DeviceArraySize,
		                         sizeof(RDPDR_DEVICE*));

		if (!_settings->DeviceArray && _settings->DeviceArraySize)
		{
			_settings->DeviceCount = 0;
			_settings->DeviceArraySize = 0;
			goto out_fail;
		}

		if (_settings->DeviceArraySize < _settings->DeviceCount)
		{
			_settings->DeviceCount = 0;
			_settings->DeviceArraySize = 0;
			goto out_fail;
		}

		for (index = 0; index < _settings->DeviceCount; index++)
		{
			_settings->DeviceArray[index] = freerdp_device_clone(
			                                    settings->DeviceArray[index]);

			if (!_settings->DeviceArray[index])
				goto out_fail;
		}

		_settings->StaticChannelCount = settings->StaticChannelCount;
		_settings->StaticChannelArraySize = settings->StaticChannelArraySize;
		_settings->StaticChannelArray = (ADDIN_ARGV**) calloc(
		                                    _settings->StaticChannelArraySize, sizeof(ADDIN_ARGV*));

		if (!_settings->StaticChannelArray && _settings->StaticChannelArraySize)
		{
			_settings->StaticChannelArraySize  = 0;
			_settings->ChannelCount = 0;
			goto out_fail;
		}

		if (_settings->StaticChannelArraySize < _settings->StaticChannelCount)
		{
			_settings->StaticChannelArraySize  = 0;
			_settings->ChannelCount = 0;
			goto out_fail;
		}

		for (index = 0; index < _settings->StaticChannelCount; index++)
		{
			_settings->StaticChannelArray[index] = freerdp_static_channel_clone(
			        settings->StaticChannelArray[index]);

			if (!_settings->StaticChannelArray[index])
				goto out_fail;
		}

		_settings->DynamicChannelCount = settings->DynamicChannelCount;
		_settings->DynamicChannelArraySize = settings->DynamicChannelArraySize;
		_settings->DynamicChannelArray = (ADDIN_ARGV**) calloc(
		                                     _settings->DynamicChannelArraySize, sizeof(ADDIN_ARGV*));

		if (!_settings->DynamicChannelArray && _settings->DynamicChannelArraySize)
		{
			_settings->DynamicChannelCount = 0;
			_settings->DynamicChannelArraySize = 0;
			goto out_fail;
		}

		if (_settings->DynamicChannelArraySize < _settings->DynamicChannelCount)
		{
			_settings->DynamicChannelCount = 0;
			_settings->DynamicChannelArraySize = 0;
			goto out_fail;
		}

		for (index = 0; index < _settings->DynamicChannelCount; index++)
		{
			_settings->DynamicChannelArray[index] = freerdp_dynamic_channel_clone(
			        settings->DynamicChannelArray[index]);

			if (!_settings->DynamicChannelArray[index])
				goto out_fail;
		}

		_settings->SettingsModified = (BYTE*) calloc(1, sizeof(rdpSettings) / 8);

		if (!_settings->SettingsModified)
			goto out_fail;
	}

	return _settings;
out_fail:
	/* In case any memory allocation failed during clone, some bytes might leak.
	 *
	 * freerdp_settings_free can't be reliable used at this point since it could
	 * free memory of pointers copied by CopyMemory and detecting and freeing
	 * each allocation separately is quite painful.
	 */
	free(_settings);
	return NULL;
}

void freerdp_settings_free(rdpSettings* settings)
{
	if (!settings)
		return;

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
	free(settings->NtlmSamFile);
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
	free(settings->CertificateContent);
	free(settings->PrivateKeyContent);
	free(settings->RdpKeyContent);
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
	free(settings->RemoteAssistancePassword);
	free(settings->RemoteAssistancePassStub);
	free(settings->RemoteAssistanceRCTicket);
	free(settings->AuthenticationServiceClass);
	free(settings->GatewayHostname);
	free(settings->GatewayUsername);
	free(settings->GatewayPassword);
	free(settings->GatewayDomain);
	free(settings->CertificateName);
	free(settings->DynamicDSTTimeZoneKeyName);
	free(settings->PreconnectionBlob);
	free(settings->KerberosKdc);
	free(settings->KerberosRealm);
	free(settings->DumpRemoteFxFile);
	free(settings->PlayRemoteFxFile);
	free(settings->RemoteApplicationName);
	free(settings->RemoteApplicationIcon);
	free(settings->RemoteApplicationProgram);
	free(settings->RemoteApplicationFile);
	free(settings->RemoteApplicationGuid);
	free(settings->RemoteApplicationCmdLine);
	free(settings->ImeFileName);
	free(settings->DrivesToRedirect);
	free(settings->WindowTitle);
	free(settings->WmClass);
	free(settings->ActionScript);
	freerdp_target_net_addresses_free(settings);
	freerdp_device_collection_free(settings);
	freerdp_static_channel_collection_free(settings);
	freerdp_dynamic_channel_collection_free(settings);
	free(settings->SettingsModified);
	free(settings);
}

#ifdef _WIN32
#pragma warning(pop)
#endif

