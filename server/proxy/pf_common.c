#include "pf_common.h"

BOOL pf_common_connection_aborted_by_peer(proxyContext* context)
{
	return WaitForSingleObject(context->connectionClosed, 0) == WAIT_OBJECT_0;
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