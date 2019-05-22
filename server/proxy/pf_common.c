/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 * Copyright 2019 Idan Freiberg <speidy@gmail.com>
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

#include "pf_common.h"

BOOL pf_common_connection_aborted_by_peer(proxyData* pdata)
{
	return WaitForSingleObject(pdata->connectionClosed, 0) == WAIT_OBJECT_0;
}

void pf_common_copy_settings(rdpSettings* dst, rdpSettings* src)
{
	/* Client/server CORE options */
	dst->RdpVersion = src->RdpVersion;
	dst->DesktopWidth = src->DesktopWidth;
	dst->DesktopHeight = src->DesktopHeight;
	dst->ColorDepth = src->ColorDepth;
	dst->ConnectionType = src->ConnectionType;
	dst->ClientBuild = src->ClientBuild;
	dst->ClientHostname = _strdup(src->ClientHostname);
	dst->ClientProductId = _strdup(src->ClientProductId);
	dst->EarlyCapabilityFlags = src->EarlyCapabilityFlags;
	dst->NetworkAutoDetect = src->NetworkAutoDetect;
	dst->SupportAsymetricKeys = src->SupportAsymetricKeys;
	dst->SupportErrorInfoPdu = src->SupportErrorInfoPdu;
	dst->SupportStatusInfoPdu = src->SupportStatusInfoPdu;
	dst->SupportMonitorLayoutPdu = src->SupportMonitorLayoutPdu;
	dst->SupportGraphicsPipeline = src->SupportGraphicsPipeline;
	dst->SupportDynamicTimeZone = src->SupportDynamicTimeZone;
	dst->SupportHeartbeatPdu = src->SupportHeartbeatPdu;
	dst->DesktopPhysicalWidth = src->DesktopPhysicalWidth;
	dst->DesktopPhysicalHeight = src->DesktopPhysicalHeight;
	dst->DesktopOrientation = src->DesktopOrientation;
	dst->DesktopScaleFactor = src->DesktopScaleFactor;
	dst->DeviceScaleFactor = src->DeviceScaleFactor;
	dst->SupportMonitorLayoutPdu = src->SupportMonitorLayoutPdu;
	/* client info */
	dst->AutoLogonEnabled = src->AutoLogonEnabled;
	dst->CompressionEnabled = src->CompressionEnabled;
	dst->DisableCtrlAltDel = src->DisableCtrlAltDel;
	dst->EnableWindowsKey = src->EnableWindowsKey;
	dst->MaximizeShell = src->MaximizeShell;
	dst->LogonNotify = src->LogonNotify;
	dst->LogonErrors = src->LogonErrors;
	dst->MouseAttached = src->MouseAttached;
	dst->MouseHasWheel = src->MouseHasWheel;
	dst->RemoteConsoleAudio = src->RemoteConsoleAudio;
	dst->AudioPlayback = src->AudioPlayback;
	dst->AudioCapture = src->AudioCapture;
	dst->VideoDisable = src->VideoDisable;
	dst->PasswordIsSmartcardPin = src->PasswordIsSmartcardPin;
	dst->UsingSavedCredentials = src->UsingSavedCredentials;
	dst->ForceEncryptedCsPdu = src->ForceEncryptedCsPdu;
	dst->HiDefRemoteApp = src->HiDefRemoteApp;
	dst->CompressionLevel = src->CompressionLevel;
	dst->PerformanceFlags = src->PerformanceFlags;
	dst->AllowFontSmoothing = src->AllowFontSmoothing;
	dst->DisableWallpaper = src->DisableWallpaper;
	dst->DisableFullWindowDrag = src->DisableFullWindowDrag;
	dst->DisableMenuAnims = src->DisableMenuAnims;
	dst->DisableThemes = src->DisableThemes;
	dst->DisableCursorShadow = src->DisableCursorShadow;
	dst->DisableCursorBlinking = src->DisableCursorBlinking;
	dst->AllowDesktopComposition = src->AllowDesktopComposition;
	dst->DisableThemes = src->DisableThemes;
	/* Remote App */
	dst->RemoteApplicationMode = src->RemoteApplicationMode;
	dst->RemoteApplicationName = src->RemoteApplicationName;
	dst->RemoteApplicationIcon = src->RemoteApplicationIcon;
	dst->RemoteApplicationProgram = src->RemoteApplicationProgram;
	dst->RemoteApplicationFile = src->RemoteApplicationFile;
	dst->RemoteApplicationGuid = src->RemoteApplicationGuid;
	dst->RemoteApplicationCmdLine = src->RemoteApplicationCmdLine;
	dst->RemoteApplicationExpandCmdLine = src->RemoteApplicationExpandCmdLine;
	dst->RemoteApplicationExpandWorkingDir = src->RemoteApplicationExpandWorkingDir;
	dst->DisableRemoteAppCapsCheck = src->DisableRemoteAppCapsCheck;
	dst->RemoteAppNumIconCaches = src->RemoteAppNumIconCaches;
	dst->RemoteAppNumIconCacheEntries = src->RemoteAppNumIconCacheEntries;
	dst->RemoteAppLanguageBarSupported = src->RemoteAppLanguageBarSupported;
	dst->RemoteWndSupportLevel = src->RemoteWndSupportLevel;
	/* GFX */
	dst->GfxThinClient = src->GfxThinClient;
	dst->GfxSmallCache = src->GfxSmallCache;
	dst->GfxProgressive = src->GfxProgressive;
	dst->GfxProgressiveV2 = src->GfxProgressiveV2;
	dst->GfxH264 = src->GfxH264;
	dst->GfxAVC444 = src->GfxAVC444;
	dst->GfxSendQoeAck = src->GfxSendQoeAck;
	dst->GfxAVC444v2 = src->GfxAVC444v2;
	dst->SupportDisplayControl = src->SupportDisplayControl;
	dst->SupportMonitorLayoutPdu = src->SupportMonitorLayoutPdu;
	dst->DynamicResolutionUpdate = src->DynamicResolutionUpdate;
	dst->DesktopResize = src->DesktopResize;
}
