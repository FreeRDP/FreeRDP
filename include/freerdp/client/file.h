/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * .rdp file
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CLIENT_RDP_FILE_H
#define FREERDP_CLIENT_RDP_FILE_H

#include <freerdp/api.h>
#include <freerdp/freerdp.h>

struct rdp_file
{
	DWORD UseMultiMon; /* use multimon */
	DWORD ScreenModeId; /* screen mode id */
	DWORD SpanMonitors; /* span monitors */
	DWORD SmartSizing; /* smartsizing */
	DWORD EnableSuperSpan; /* enablesuperpan */
	DWORD SuperSpanAccelerationFactor; /* superpanaccelerationfactor */

	DWORD DesktopWidth; /* desktopwidth */
	DWORD DesktopHeight; /* desktopheight */
	DWORD DesktopSizeId; /* desktop size id */
	DWORD SessionBpp; /* session bpp */

	DWORD Compression; /* compression */
	DWORD KeyboardHook; /* keyboardhook */
	DWORD DisableCtrlAltDel; /* disable ctrl+alt+del */

	DWORD AudioMode; /* audiomode */
	DWORD AudioQualityMode; /* audioqualitymode */
	DWORD AudioCaptureMode; /* audiocapturemode */
	DWORD VideoPlaybackMode; /* videoplaybackmode */

	DWORD ConnectionType; /* connection type */

	DWORD NetworkAutoDetect; /* networkautodetect */
	DWORD BandwidthAutoDetect; /* bandwidthautodetect */

	DWORD PinConnectionBar; /* pinconnectionbar */
	DWORD DisplayConnectionBar; /* displayconnectionbar */

	DWORD WorkspaceId; /* workspaceid */
	DWORD EnableWorkspaceReconnect; /* enableworkspacereconnect */

	DWORD DisableWallpaper; /* disable wallpaper */
	DWORD AllowFontSmoothing; /* allow font smoothing */
	DWORD AllowDesktopComposition; /* allow desktop composition */
	DWORD DisableFullWindowDrag; /* disable full window drag */
	DWORD DisableMenuAnims; /* disable menu anims */
	DWORD DisableThemes; /* disable themes */
	DWORD DisableCursorSetting; /* disable cursor setting */

	DWORD BitmapCacheSize; /* bitmapcachesize */
	DWORD BitmapCachePersistEnable; /* bitmapcachepersistenable */

	LPSTR Username; /* username */
	LPSTR Domain; /* domain */
	PBYTE Password51; /* password 51 */

	LPSTR FullAddress; /* full address */
	LPSTR AlternateFullAddress; /* alternate full address */
	DWORD ServerPort; /* server port */

	DWORD RedirectDrives; /* redirectdrives */
	DWORD RedirectPrinters; /* redirectprinters */
	DWORD RedirectComPorts; /* redirectcomports */
	DWORD RedirectSmartCards; /* redirectsmartcards */
	DWORD RedirectClipboard; /* redirectclipboard */
	DWORD RedirectPosDevices; /* redirectposdevices */
	DWORD RedirectDirectX; /* redirectdirectx */
	DWORD DisablePrinterRedirection; /* disableprinterredirection */
	DWORD DisableClipboardRedirection; /* disableclipboardredirection */
	LPSTR UsbDevicesToRedirect; /* usbdevicestoredirect */

	DWORD ConnectToConsole; /* connect to console */
	DWORD AdministrativeSession; /* administrative session */
	DWORD AutoReconnectionEnabled; /* autoreconnection enabled */
	DWORD AutoReconnectMaxRetries; /* autoreconnect max retries */

	DWORD PublicMode; /* public mode */
	DWORD AuthenticationLevel; /* authentication level */
	DWORD PromptCredentialOnce; /* promptcredentialonce */
	DWORD PromptForCredentials; /* prompt for credentials */
	DWORD PromptForCredentialsOnce; /* promptcredentialonce */
	DWORD NegotiateSecurityLayer; /* negotiate security layer */
	DWORD EnableCredSSPSupport; /* enablecredsspsupport */
	LPSTR LoadBalanceInfo; /* loadbalanceinfo */

	DWORD RemoteApplicationMode; /* remoteapplicationmode */
	LPSTR RemoteApplicationName; /* remoteapplicationname */
	LPSTR RemoteApplicationIcon; /* remoteapplicationicon */
	LPSTR RemoteApplicationProgram; /* remoteapplicationprogram */
	LPSTR RemoteApplicationFile; /* remoteapplicationfile */
	LPSTR RemoteApplicationGuid; /* remoteapplicationguid */
	LPSTR RemoteApplicationCmdLine; /* remoteapplicationcmdline */
	DWORD RemoteApplicationExpandCmdLine; /* remoteapplicationexpandcmdline */
	DWORD RemoteApplicationExpandWorkingDir; /* remoteapplicationexpandworkingdir */
	DWORD DisableConnectionSharing; /* disableconnectionsharing */
	DWORD DisableRemoteAppCapsCheck; /* disableremoteappcapscheck */

	LPSTR AlternateShell; /* alternate shell */
	LPSTR ShellWorkingDirectory; /* shell working directory */

	LPSTR GatewayHostname; /* gatewayhostname */
	DWORD GatewayUsageMethod; /* gatewayusagemethod */
	DWORD GatewayProfileUsageMethod; /* gatewayprofileusagemethod */
	DWORD GatewayCredentialsSource; /* gatewaycredentialssource */

	DWORD UseRedirectionServerName; /* use redirection server name */

	DWORD RdgIsKdcProxy; /* rdgiskdcproxy */
	LPSTR KdcProxyName; /* kdcproxyname */

	LPSTR DrivesToRedirect; /* drivestoredirect */
	LPSTR DevicesToRedirect; /* devicestoredirect */
	LPSTR WinPosStr; /* winposstr */

	int argc;
	char** argv;
	int argSize;
};

typedef struct rdp_file rdpFile;

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API BOOL freerdp_client_parse_rdp_file(rdpFile* file, const char* name);
FREERDP_API BOOL freerdp_client_parse_rdp_file_buffer(rdpFile* file, BYTE* buffer, size_t size);
FREERDP_API BOOL freerdp_client_populate_settings_from_rdp_file(rdpFile* file, rdpSettings* settings);

FREERDP_API BOOL freerdp_client_populate_rdp_file_from_settings(rdpFile* file, rdpSettings* settings);
FREERDP_API BOOL freerdp_client_write_rdp_file(rdpFile* file, const char* name, BOOL unicode);
FREERDP_API size_t freerdp_client_write_rdp_file_buffer(rdpFile* file, char* buffer, size_t size);

FREERDP_API rdpFile* freerdp_client_rdp_file_new(void);
FREERDP_API void freerdp_client_rdp_file_free(rdpFile* file);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CLIENT_RDP_FILE_H */
