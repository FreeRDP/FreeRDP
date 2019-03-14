/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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

#include "pf_client.h"
#include "pf_context.h"

/* Proxy context initialization callback */
BOOL client_to_proxy_context_new(freerdp_peer* client, clientToProxyContext* context)
{
	context->vcm = WTSOpenServerA((LPSTR) client->context);

	if (!context->vcm || context->vcm == INVALID_HANDLE_VALUE)
		goto fail_open_server;

	return TRUE;
fail_open_server:
	context->vcm = NULL;
	return FALSE;
}

/* Proxy context free callback */
void client_to_proxy_context_free(freerdp_peer* client, clientToProxyContext* context)
{
	if (context)
	{
		WTSCloseServer((HANDLE) context->vcm);
	}
}

void proxy_settings_mirror(rdpSettings* settings, rdpSettings* baseSettings)
{
	/* Client/server CORE options */
	settings->RdpVersion = baseSettings->RdpVersion;
	settings->DesktopWidth = baseSettings->DesktopWidth;
	settings->DesktopHeight = baseSettings->DesktopHeight;
	settings->ColorDepth = baseSettings->ColorDepth;
	settings->ConnectionType = baseSettings->ConnectionType;
	settings->ClientBuild = baseSettings->ClientBuild;
	settings->ClientHostname = _strdup(baseSettings->ClientHostname);
	settings->ClientProductId = _strdup(baseSettings->ClientProductId);
	settings->EarlyCapabilityFlags = baseSettings->EarlyCapabilityFlags;
	settings->NetworkAutoDetect = baseSettings->NetworkAutoDetect;
	settings->SupportAsymetricKeys = baseSettings->SupportAsymetricKeys;
	settings->SupportErrorInfoPdu = baseSettings->SupportErrorInfoPdu;
	settings->SupportStatusInfoPdu = baseSettings->SupportStatusInfoPdu;
	settings->SupportMonitorLayoutPdu = baseSettings->SupportMonitorLayoutPdu;
	settings->SupportGraphicsPipeline = baseSettings->SupportGraphicsPipeline;
	settings->SupportDynamicTimeZone = baseSettings->SupportDynamicTimeZone;
	settings->SupportHeartbeatPdu = baseSettings->SupportHeartbeatPdu;
	settings->DesktopPhysicalWidth = baseSettings->DesktopPhysicalWidth;
	settings->DesktopPhysicalHeight = baseSettings->DesktopPhysicalHeight;
	settings->DesktopOrientation = baseSettings->DesktopOrientation;
	settings->DesktopScaleFactor = baseSettings->DesktopScaleFactor;
	settings->DeviceScaleFactor = baseSettings->DeviceScaleFactor;
	/* client info */
	settings->AutoLogonEnabled = baseSettings->AutoLogonEnabled;
	settings->CompressionEnabled = baseSettings->CompressionEnabled;
	settings->DisableCtrlAltDel = baseSettings->DisableCtrlAltDel;
	settings->EnableWindowsKey = baseSettings->EnableWindowsKey;
	settings->MaximizeShell = baseSettings->MaximizeShell;
	settings->LogonNotify = baseSettings->LogonNotify;
	settings->LogonErrors = baseSettings->LogonErrors;
	settings->MouseAttached = baseSettings->MouseAttached;
	settings->MouseHasWheel = baseSettings->MouseHasWheel;
	settings->RemoteConsoleAudio = baseSettings->RemoteConsoleAudio;
	settings->AudioPlayback = baseSettings->AudioPlayback;
	settings->AudioCapture = baseSettings->AudioCapture;
	settings->VideoDisable = baseSettings->VideoDisable;
	settings->PasswordIsSmartcardPin = baseSettings->PasswordIsSmartcardPin;
	settings->UsingSavedCredentials = baseSettings->UsingSavedCredentials;
	settings->ForceEncryptedCsPdu = baseSettings->ForceEncryptedCsPdu;
	settings->HiDefRemoteApp = baseSettings->HiDefRemoteApp;
	settings->CompressionLevel = baseSettings->CompressionLevel;
	settings->PerformanceFlags = baseSettings->PerformanceFlags;
	settings->AllowFontSmoothing = baseSettings->AllowFontSmoothing;
	settings->DisableWallpaper = baseSettings->DisableWallpaper;
	settings->DisableFullWindowDrag = baseSettings->DisableFullWindowDrag;
	settings->DisableMenuAnims = baseSettings->DisableMenuAnims;
	settings->DisableThemes = baseSettings->DisableThemes;
	settings->DisableCursorShadow = baseSettings->DisableCursorShadow;
	settings->DisableCursorBlinking = baseSettings->DisableCursorBlinking;
	settings->AllowDesktopComposition = baseSettings->AllowDesktopComposition;
	settings->DisableThemes = baseSettings->DisableThemes;
	/* Remote App */
	settings->RemoteApplicationMode = baseSettings->RemoteApplicationMode;
	settings->RemoteApplicationName = baseSettings->RemoteApplicationName;
	settings->RemoteApplicationIcon = baseSettings->RemoteApplicationIcon;
	settings->RemoteApplicationProgram = baseSettings->RemoteApplicationProgram;
	settings->RemoteApplicationFile = baseSettings->RemoteApplicationFile;
	settings->RemoteApplicationGuid = baseSettings->RemoteApplicationGuid;
	settings->RemoteApplicationCmdLine = baseSettings->RemoteApplicationCmdLine;
	settings->RemoteApplicationExpandCmdLine = baseSettings->RemoteApplicationExpandCmdLine;
	settings->RemoteApplicationExpandWorkingDir = baseSettings->RemoteApplicationExpandWorkingDir;
	settings->DisableRemoteAppCapsCheck = baseSettings->DisableRemoteAppCapsCheck;
	settings->RemoteAppNumIconCaches = baseSettings->RemoteAppNumIconCaches;
	settings->RemoteAppNumIconCacheEntries = baseSettings->RemoteAppNumIconCacheEntries;
	settings->RemoteAppLanguageBarSupported = baseSettings->RemoteAppLanguageBarSupported;
	settings->RemoteWndSupportLevel = baseSettings->RemoteWndSupportLevel;
	/* GFX */
	settings->GfxThinClient = baseSettings->GfxThinClient;
	settings->GfxSmallCache = baseSettings->GfxSmallCache;
	settings->GfxProgressive = baseSettings->GfxProgressive;
	settings->GfxProgressiveV2 = baseSettings->GfxProgressiveV2;
	settings->GfxH264 = baseSettings->GfxH264;
	settings->GfxAVC444 = baseSettings->GfxAVC444;
	settings->GfxSendQoeAck = baseSettings->GfxSendQoeAck;
	settings->GfxAVC444v2 = baseSettings->GfxAVC444v2;
}

rdpContext* proxy_to_server_context_create(rdpSettings* baseSettings, char* host,
        DWORD port, char* username, char* password)
{
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints;
	rdpContext* context;
	rdpSettings* settings;
	RdpClientEntry(&clientEntryPoints);
	context = freerdp_client_context_new(&clientEntryPoints);

	if (!context)
		return NULL;

	settings = context->settings;
	proxy_settings_mirror(settings, baseSettings);
	settings->Username = _strdup(baseSettings->Username);
	settings->Password = _strdup(baseSettings->Password);
	settings->ServerHostname = host;
	settings->ServerPort = port;
	settings->Username = username;
	settings->Password = password;
	settings->SoftwareGdi = FALSE;
	settings->RedirectClipboard = FALSE;
	return context;
}