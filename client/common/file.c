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

#include <freerdp/config.h>

#include <errno.h>
#include <ctype.h>
#include <stdlib.h>

#include <winpr/string.h>
#include <winpr/file.h>

#include <freerdp/client.h>
#include <freerdp/client/file.h>
#include <freerdp/client/cmdline.h>

#include <freerdp/channels/urbdrc.h>
#include <freerdp/channels/rdpecam.h>
#include <freerdp/channels/location.h>

/**
 * Remote Desktop Plus - Overview of .rdp file settings:
 * http://www.donkz.nl/files/rdpsettings.html
 *
 * RDP Settings for Remote Desktop Services in Windows Server 2008 R2:
 * http://technet.microsoft.com/en-us/library/ff393699/
 *
 * https://docs.microsoft.com/en-us/windows-server/remote/remote-desktop-services/clients/rdp-files
 */

#include <stdio.h>
#include <string.h>

#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <freerdp/log.h>
#define TAG CLIENT_TAG("common")

/*#define DEBUG_CLIENT_FILE	1*/

static const BYTE BOM_UTF16_LE[2] = { 0xFF, 0xFE };

// #define INVALID_INTEGER_VALUE 0xFFFFFFFF

#define RDP_FILE_LINE_FLAG_FORMATTED 0x00000001
// #define RDP_FILE_LINE_FLAG_STANDARD 0x00000002
#define RDP_FILE_LINE_FLAG_TYPE_STRING 0x00000010
#define RDP_FILE_LINE_FLAG_TYPE_INTEGER 0x00000020
// #define RDP_FILE_LINE_FLAG_TYPE_BINARY 0x00000040

struct rdp_file_line
{
	char* name;
	LPSTR sValue;
	PBYTE bValue;

	size_t index;

	long iValue;
	DWORD flags;
	int valueLength;
};
typedef struct rdp_file_line rdpFileLine;

struct rdp_file
{
	DWORD UseMultiMon;                 /* use multimon */
	LPSTR SelectedMonitors;            /* selectedmonitors */
	DWORD MaximizeToCurrentDisplays;   /* maximizetocurrentdisplays */
	DWORD SingleMonInWindowedMode;     /* singlemoninwindowedmode */
	DWORD ScreenModeId;                /* screen mode id */
	DWORD SpanMonitors;                /* span monitors */
	DWORD SmartSizing;                 /* smartsizing */
	DWORD DynamicResolution;           /* dynamic resolution */
	DWORD EnableSuperSpan;             /* enablesuperpan */
	DWORD SuperSpanAccelerationFactor; /* superpanaccelerationfactor */

	DWORD DesktopWidth;       /* desktopwidth */
	DWORD DesktopHeight;      /* desktopheight */
	DWORD DesktopSizeId;      /* desktop size id */
	DWORD SessionBpp;         /* session bpp */
	DWORD DesktopScaleFactor; /* desktopscalefactor */

	DWORD Compression;       /* compression */
	DWORD KeyboardHook;      /* keyboardhook */
	DWORD DisableCtrlAltDel; /* disable ctrl+alt+del */

	DWORD AudioMode;                             /* audiomode */
	DWORD AudioQualityMode;                      /* audioqualitymode */
	DWORD AudioCaptureMode;                      /* audiocapturemode */
	DWORD EncodeRedirectedVideoCapture;          /* encode redirected video capture */
	DWORD RedirectedVideoCaptureEncodingQuality; /* redirected video capture encoding quality */
	DWORD VideoPlaybackMode;                     /* videoplaybackmode */

	DWORD ConnectionType; /* connection type */

	DWORD NetworkAutoDetect;   /* networkautodetect */
	DWORD BandwidthAutoDetect; /* bandwidthautodetect */

	DWORD PinConnectionBar;     /* pinconnectionbar */
	DWORD DisplayConnectionBar; /* displayconnectionbar */

	DWORD WorkspaceId;              /* workspaceid */
	DWORD EnableWorkspaceReconnect; /* enableworkspacereconnect */

	DWORD DisableWallpaper;        /* disable wallpaper */
	DWORD AllowFontSmoothing;      /* allow font smoothing */
	DWORD AllowDesktopComposition; /* allow desktop composition */
	DWORD DisableFullWindowDrag;   /* disable full window drag */
	DWORD DisableMenuAnims;        /* disable menu anims */
	DWORD DisableThemes;           /* disable themes */
	DWORD DisableCursorSetting;    /* disable cursor setting */

	DWORD BitmapCacheSize;          /* bitmapcachesize */
	DWORD BitmapCachePersistEnable; /* bitmapcachepersistenable */

	DWORD ServerPort; /* server port */

	LPSTR Username;   /* username */
	LPSTR Domain;     /* domain */
	LPSTR Password;   /*password*/
	PBYTE Password51; /* password 51 */

	LPSTR FullAddress;          /* full address */
	LPSTR AlternateFullAddress; /* alternate full address */

	LPSTR UsbDevicesToRedirect;        /* usbdevicestoredirect */
	DWORD RedirectDrives;              /* redirectdrives */
	DWORD RedirectPrinters;            /* redirectprinters */
	DWORD RedirectComPorts;            /* redirectcomports */
	DWORD RedirectLocation;            /* redirectlocation */
	DWORD RedirectSmartCards;          /* redirectsmartcards */
	DWORD RedirectWebauthN;            /* redirectwebauthn */
	LPSTR RedirectCameras;             /* camerastoredirect */
	DWORD RedirectClipboard;           /* redirectclipboard */
	DWORD RedirectPosDevices;          /* redirectposdevices */
	DWORD RedirectDirectX;             /* redirectdirectx */
	DWORD DisablePrinterRedirection;   /* disableprinterredirection */
	DWORD DisableClipboardRedirection; /* disableclipboardredirection */

	DWORD ConnectToConsole;        /* connect to console */
	DWORD AdministrativeSession;   /* administrative session */
	DWORD AutoReconnectionEnabled; /* autoreconnection enabled */
	DWORD AutoReconnectMaxRetries; /* autoreconnect max retries */

	DWORD PublicMode;             /* public mode */
	DWORD AuthenticationLevel;    /* authentication level */
	DWORD PromptCredentialOnce;   /* promptcredentialonce */
	DWORD PromptForCredentials;   /* prompt for credentials */
	DWORD NegotiateSecurityLayer; /* negotiate security layer */
	DWORD EnableCredSSPSupport;   /* enablecredsspsupport */
	DWORD EnableRdsAadAuth;       /* enablerdsaadauth */

	DWORD RemoteApplicationMode; /* remoteapplicationmode */
	LPSTR LoadBalanceInfo;       /* loadbalanceinfo */

	LPSTR RemoteApplicationName;             /* remoteapplicationname */
	LPSTR RemoteApplicationIcon;             /* remoteapplicationicon */
	LPSTR RemoteApplicationProgram;          /* remoteapplicationprogram */
	LPSTR RemoteApplicationFile;             /* remoteapplicationfile */
	LPSTR RemoteApplicationGuid;             /* remoteapplicationguid */
	LPSTR RemoteApplicationCmdLine;          /* remoteapplicationcmdline */
	DWORD RemoteApplicationExpandCmdLine;    /* remoteapplicationexpandcmdline */
	DWORD RemoteApplicationExpandWorkingDir; /* remoteapplicationexpandworkingdir */
	DWORD DisableConnectionSharing;          /* disableconnectionsharing */
	DWORD DisableRemoteAppCapsCheck;         /* disableremoteappcapscheck */

	LPSTR AlternateShell;        /* alternate shell */
	LPSTR ShellWorkingDirectory; /* shell working directory */

	LPSTR GatewayHostname;           /* gatewayhostname */
	DWORD GatewayUsageMethod;        /* gatewayusagemethod */
	DWORD GatewayProfileUsageMethod; /* gatewayprofileusagemethod */
	DWORD GatewayCredentialsSource;  /* gatewaycredentialssource */

	LPSTR ResourceProvider; /* resourceprovider */

	LPSTR WvdEndpointPool;      /* wvd endpoint pool */
	LPSTR geo;                  /* geo */
	LPSTR armpath;              /* armpath */
	LPSTR aadtenantid;          /* aadtenantid" */
	LPSTR diagnosticserviceurl; /* diagnosticserviceurl */
	LPSTR hubdiscoverygeourl;   /* hubdiscoverygeourl" */
	LPSTR activityhint;         /* activityhint */

	DWORD UseRedirectionServerName; /* use redirection server name */

	LPSTR GatewayAccessToken; /* gatewayaccesstoken */

	LPSTR DrivesToRedirect;  /* drivestoredirect */
	LPSTR DevicesToRedirect; /* devicestoredirect */
	LPSTR WinPosStr;         /* winposstr */

	LPSTR PreconnectionBlob; /* pcb */

	LPSTR KdcProxyName;  /* kdcproxyname */
	DWORD RdgIsKdcProxy; /* rdgiskdcproxy */

	DWORD align1;

	size_t lineCount;
	size_t lineSize;
	rdpFileLine* lines;

	ADDIN_ARGV* args;
	void* context;

	DWORD flags;
};

static const char key_str_username[] = "username";
static const char key_str_domain[] = "domain";
static const char key_str_password[] = "password";
static const char key_str_full_address[] = "full address";
static const char key_str_alternate_full_address[] = "alternate full address";
static const char key_str_usbdevicestoredirect[] = "usbdevicestoredirect";
static const char key_str_camerastoredirect[] = "camerastoredirect";
static const char key_str_loadbalanceinfo[] = "loadbalanceinfo";
static const char key_str_remoteapplicationname[] = "remoteapplicationname";
static const char key_str_remoteapplicationicon[] = "remoteapplicationicon";
static const char key_str_remoteapplicationprogram[] = "remoteapplicationprogram";
static const char key_str_remoteapplicationfile[] = "remoteapplicationfile";
static const char key_str_remoteapplicationguid[] = "remoteapplicationguid";
static const char key_str_remoteapplicationcmdline[] = "remoteapplicationcmdline";
static const char key_str_alternate_shell[] = "alternate shell";
static const char key_str_shell_working_directory[] = "shell working directory";
static const char key_str_gatewayhostname[] = "gatewayhostname";
static const char key_str_gatewayaccesstoken[] = "gatewayaccesstoken";
static const char key_str_resourceprovider[] = "resourceprovider";
static const char str_resourceprovider_arm[] = "arm";
static const char key_str_kdcproxyname[] = "kdcproxyname";
static const char key_str_drivestoredirect[] = "drivestoredirect";
static const char key_str_devicestoredirect[] = "devicestoredirect";
static const char key_str_winposstr[] = "winposstr";
static const char key_str_pcb[] = "pcb";
static const char key_str_selectedmonitors[] = "selectedmonitors";

static const char key_str_wvd[] = "wvd endpoint pool";
static const char key_str_geo[] = "geo";
static const char key_str_armpath[] = "armpath";
static const char key_str_aadtenantid[] = "aadtenantid";

static const char key_str_diagnosticserviceurl[] = "diagnosticserviceurl";
static const char key_str_hubdiscoverygeourl[] = "hubdiscoverygeourl";

static const char key_str_activityhint[] = "activityhint";

static const char key_int_rdgiskdcproxy[] = "rdgiskdcproxy";
static const char key_int_use_redirection_server_name[] = "use redirection server name";
static const char key_int_gatewaycredentialssource[] = "gatewaycredentialssource";
static const char key_int_gatewayprofileusagemethod[] = "gatewayprofileusagemethod";
static const char key_int_gatewayusagemethod[] = "gatewayusagemethod";
static const char key_int_disableremoteappcapscheck[] = "disableremoteappcapscheck";
static const char key_int_disableconnectionsharing[] = "disableconnectionsharing";
static const char key_int_remoteapplicationexpandworkingdir[] = "remoteapplicationexpandworkingdir";
static const char key_int_remoteapplicationexpandcmdline[] = "remoteapplicationexpandcmdline";
static const char key_int_remoteapplicationmode[] = "remoteapplicationmode";
static const char key_int_enablecredsspsupport[] = "enablecredsspsupport";
static const char key_int_enablerdsaadauth[] = "enablerdsaadauth";
static const char key_int_negotiate_security_layer[] = "negotiate security layer";
static const char key_int_prompt_for_credentials[] = "prompt for credentials";
static const char key_int_promptcredentialonce[] = "promptcredentialonce";
static const char key_int_authentication_level[] = "authentication level";
static const char key_int_public_mode[] = "public mode";
static const char key_int_autoreconnect_max_retries[] = "autoreconnect max retries";
static const char key_int_autoreconnection_enabled[] = "autoreconnection enabled";
static const char key_int_administrative_session[] = "administrative session";
static const char key_int_connect_to_console[] = "connect to console";
static const char key_int_disableclipboardredirection[] = "disableclipboardredirection";
static const char key_int_disableprinterredirection[] = "disableprinterredirection";
static const char key_int_redirectdirectx[] = "redirectdirectx";
static const char key_int_redirectposdevices[] = "redirectposdevices";
static const char key_int_redirectclipboard[] = "redirectclipboard";
static const char key_int_redirectsmartcards[] = "redirectsmartcards";
static const char key_int_redirectcomports[] = "redirectcomports";
static const char key_int_redirectlocation[] = "redirectlocation";
static const char key_int_redirectprinters[] = "redirectprinters";
static const char key_int_redirectdrives[] = "redirectdrives";
static const char key_int_server_port[] = "server port";
static const char key_int_bitmapcachepersistenable[] = "bitmapcachepersistenable";
static const char key_int_bitmapcachesize[] = "bitmapcachesize";
static const char key_int_disable_cursor_setting[] = "disable cursor setting";
static const char key_int_disable_themes[] = "disable themes";
static const char key_int_disable_menu_anims[] = "disable menu anims";
static const char key_int_disable_full_window_drag[] = "disable full window drag";
static const char key_int_allow_desktop_composition[] = "allow desktop composition";
static const char key_int_allow_font_smoothing[] = "allow font smoothing";
static const char key_int_disable_wallpaper[] = "disable wallpaper";
static const char key_int_enableworkspacereconnect[] = "enableworkspacereconnect";
static const char key_int_workspaceid[] = "workspaceid";
static const char key_int_displayconnectionbar[] = "displayconnectionbar";
static const char key_int_pinconnectionbar[] = "pinconnectionbar";
static const char key_int_bandwidthautodetect[] = "bandwidthautodetect";
static const char key_int_networkautodetect[] = "networkautodetect";
static const char key_int_connection_type[] = "connection type";
static const char key_int_videoplaybackmode[] = "videoplaybackmode";
static const char key_int_redirected_video_capture_encoding_quality[] =
    "redirected video capture encoding quality";
static const char key_int_encode_redirected_video_capture[] = "encode redirected video capture";
static const char key_int_audiocapturemode[] = "audiocapturemode";
static const char key_int_audioqualitymode[] = "audioqualitymode";
static const char key_int_audiomode[] = "audiomode";
static const char key_int_disable_ctrl_alt_del[] = "disable ctrl+alt+del";
static const char key_int_keyboardhook[] = "keyboardhook";
static const char key_int_compression[] = "compression";
static const char key_int_desktopscalefactor[] = "desktopscalefactor";
static const char key_int_session_bpp[] = "session bpp";
static const char key_int_desktop_size_id[] = "desktop size id";
static const char key_int_desktopheight[] = "desktopheight";
static const char key_int_desktopwidth[] = "desktopwidth";
static const char key_int_superpanaccelerationfactor[] = "superpanaccelerationfactor";
static const char key_int_enablesuperpan[] = "enablesuperpan";
static const char key_int_dynamic_resolution[] = "dynamic resolution";
static const char key_int_smart_sizing[] = "smart sizing";
static const char key_int_span_monitors[] = "span monitors";
static const char key_int_screen_mode_id[] = "screen mode id";
static const char key_int_singlemoninwindowedmode[] = "singlemoninwindowedmode";
static const char key_int_maximizetocurrentdisplays[] = "maximizetocurrentdisplays";
static const char key_int_use_multimon[] = "use multimon";
static const char key_int_redirectwebauthn[] = "redirectwebauthn";

static BOOL utils_str_is_empty(const char* str)
{
	if (!str)
		return TRUE;
	if (strlen(str) == 0)
		return TRUE;
	return FALSE;
}

static SSIZE_T freerdp_client_rdp_file_add_line(rdpFile* file);
static rdpFileLine* freerdp_client_rdp_file_find_line_by_name(const rdpFile* file,
                                                              const char* name);
static void freerdp_client_file_string_check_free(LPSTR str);

static BOOL freerdp_client_rdp_file_find_integer_entry(rdpFile* file, const char* name,
                                                       DWORD** outValue, rdpFileLine** outLine)
{
	WINPR_ASSERT(file);
	WINPR_ASSERT(name);
	WINPR_ASSERT(outValue);
	WINPR_ASSERT(outLine);

	*outValue = NULL;
	*outLine = NULL;

	if (_stricmp(name, key_int_use_multimon) == 0)
		*outValue = &file->UseMultiMon;
	else if (_stricmp(name, key_int_maximizetocurrentdisplays) == 0)
		*outValue = &file->MaximizeToCurrentDisplays;
	else if (_stricmp(name, key_int_singlemoninwindowedmode) == 0)
		*outValue = &file->SingleMonInWindowedMode;
	else if (_stricmp(name, key_int_screen_mode_id) == 0)
		*outValue = &file->ScreenModeId;
	else if (_stricmp(name, key_int_span_monitors) == 0)
		*outValue = &file->SpanMonitors;
	else if (_stricmp(name, key_int_smart_sizing) == 0)
		*outValue = &file->SmartSizing;
	else if (_stricmp(name, key_int_dynamic_resolution) == 0)
		*outValue = &file->DynamicResolution;
	else if (_stricmp(name, key_int_enablesuperpan) == 0)
		*outValue = &file->EnableSuperSpan;
	else if (_stricmp(name, key_int_superpanaccelerationfactor) == 0)
		*outValue = &file->SuperSpanAccelerationFactor;
	else if (_stricmp(name, key_int_desktopwidth) == 0)
		*outValue = &file->DesktopWidth;
	else if (_stricmp(name, key_int_desktopheight) == 0)
		*outValue = &file->DesktopHeight;
	else if (_stricmp(name, key_int_desktop_size_id) == 0)
		*outValue = &file->DesktopSizeId;
	else if (_stricmp(name, key_int_session_bpp) == 0)
		*outValue = &file->SessionBpp;
	else if (_stricmp(name, key_int_desktopscalefactor) == 0)
		*outValue = &file->DesktopScaleFactor;
	else if (_stricmp(name, key_int_compression) == 0)
		*outValue = &file->Compression;
	else if (_stricmp(name, key_int_keyboardhook) == 0)
		*outValue = &file->KeyboardHook;
	else if (_stricmp(name, key_int_disable_ctrl_alt_del) == 0)
		*outValue = &file->DisableCtrlAltDel;
	else if (_stricmp(name, key_int_audiomode) == 0)
		*outValue = &file->AudioMode;
	else if (_stricmp(name, key_int_audioqualitymode) == 0)
		*outValue = &file->AudioQualityMode;
	else if (_stricmp(name, key_int_audiocapturemode) == 0)
		*outValue = &file->AudioCaptureMode;
	else if (_stricmp(name, key_int_encode_redirected_video_capture) == 0)
		*outValue = &file->EncodeRedirectedVideoCapture;
	else if (_stricmp(name, key_int_redirected_video_capture_encoding_quality) == 0)
		*outValue = &file->RedirectedVideoCaptureEncodingQuality;
	else if (_stricmp(name, key_int_videoplaybackmode) == 0)
		*outValue = &file->VideoPlaybackMode;
	else if (_stricmp(name, key_int_connection_type) == 0)
		*outValue = &file->ConnectionType;
	else if (_stricmp(name, key_int_networkautodetect) == 0)
		*outValue = &file->NetworkAutoDetect;
	else if (_stricmp(name, key_int_bandwidthautodetect) == 0)
		*outValue = &file->BandwidthAutoDetect;
	else if (_stricmp(name, key_int_pinconnectionbar) == 0)
		*outValue = &file->PinConnectionBar;
	else if (_stricmp(name, key_int_displayconnectionbar) == 0)
		*outValue = &file->DisplayConnectionBar;
	else if (_stricmp(name, key_int_workspaceid) == 0)
		*outValue = &file->WorkspaceId;
	else if (_stricmp(name, key_int_enableworkspacereconnect) == 0)
		*outValue = &file->EnableWorkspaceReconnect;
	else if (_stricmp(name, key_int_disable_wallpaper) == 0)
		*outValue = &file->DisableWallpaper;
	else if (_stricmp(name, key_int_allow_font_smoothing) == 0)
		*outValue = &file->AllowFontSmoothing;
	else if (_stricmp(name, key_int_allow_desktop_composition) == 0)
		*outValue = &file->AllowDesktopComposition;
	else if (_stricmp(name, key_int_disable_full_window_drag) == 0)
		*outValue = &file->DisableFullWindowDrag;
	else if (_stricmp(name, key_int_disable_menu_anims) == 0)
		*outValue = &file->DisableMenuAnims;
	else if (_stricmp(name, key_int_disable_themes) == 0)
		*outValue = &file->DisableThemes;
	else if (_stricmp(name, key_int_disable_cursor_setting) == 0)
		*outValue = &file->DisableCursorSetting;
	else if (_stricmp(name, key_int_bitmapcachesize) == 0)
		*outValue = &file->BitmapCacheSize;
	else if (_stricmp(name, key_int_bitmapcachepersistenable) == 0)
		*outValue = &file->BitmapCachePersistEnable;
	else if (_stricmp(name, key_int_server_port) == 0)
		*outValue = &file->ServerPort;
	else if (_stricmp(name, key_int_redirectdrives) == 0)
		*outValue = &file->RedirectDrives;
	else if (_stricmp(name, key_int_redirectprinters) == 0)
		*outValue = &file->RedirectPrinters;
	else if (_stricmp(name, key_int_redirectcomports) == 0)
		*outValue = &file->RedirectComPorts;
	else if (_stricmp(name, key_int_redirectlocation) == 0)
		*outValue = &file->RedirectLocation;
	else if (_stricmp(name, key_int_redirectsmartcards) == 0)
		*outValue = &file->RedirectSmartCards;
	else if (_stricmp(name, key_int_redirectclipboard) == 0)
		*outValue = &file->RedirectClipboard;
	else if (_stricmp(name, key_int_redirectposdevices) == 0)
		*outValue = &file->RedirectPosDevices;
	else if (_stricmp(name, key_int_redirectdirectx) == 0)
		*outValue = &file->RedirectDirectX;
	else if (_stricmp(name, key_int_disableprinterredirection) == 0)
		*outValue = &file->DisablePrinterRedirection;
	else if (_stricmp(name, key_int_disableclipboardredirection) == 0)
		*outValue = &file->DisableClipboardRedirection;
	else if (_stricmp(name, key_int_connect_to_console) == 0)
		*outValue = &file->ConnectToConsole;
	else if (_stricmp(name, key_int_administrative_session) == 0)
		*outValue = &file->AdministrativeSession;
	else if (_stricmp(name, key_int_autoreconnection_enabled) == 0)
		*outValue = &file->AutoReconnectionEnabled;
	else if (_stricmp(name, key_int_autoreconnect_max_retries) == 0)
		*outValue = &file->AutoReconnectMaxRetries;
	else if (_stricmp(name, key_int_public_mode) == 0)
		*outValue = &file->PublicMode;
	else if (_stricmp(name, key_int_authentication_level) == 0)
		*outValue = &file->AuthenticationLevel;
	else if (_stricmp(name, key_int_promptcredentialonce) == 0)
		*outValue = &file->PromptCredentialOnce;
	else if ((_stricmp(name, key_int_prompt_for_credentials) == 0))
		*outValue = &file->PromptForCredentials;
	else if (_stricmp(name, key_int_negotiate_security_layer) == 0)
		*outValue = &file->NegotiateSecurityLayer;
	else if (_stricmp(name, key_int_enablecredsspsupport) == 0)
		*outValue = &file->EnableCredSSPSupport;
	else if (_stricmp(name, key_int_enablerdsaadauth) == 0)
		*outValue = &file->EnableRdsAadAuth;
	else if (_stricmp(name, key_int_remoteapplicationmode) == 0)
		*outValue = &file->RemoteApplicationMode;
	else if (_stricmp(name, key_int_remoteapplicationexpandcmdline) == 0)
		*outValue = &file->RemoteApplicationExpandCmdLine;
	else if (_stricmp(name, key_int_remoteapplicationexpandworkingdir) == 0)
		*outValue = &file->RemoteApplicationExpandWorkingDir;
	else if (_stricmp(name, key_int_disableconnectionsharing) == 0)
		*outValue = &file->DisableConnectionSharing;
	else if (_stricmp(name, key_int_disableremoteappcapscheck) == 0)
		*outValue = &file->DisableRemoteAppCapsCheck;
	else if (_stricmp(name, key_int_gatewayusagemethod) == 0)
		*outValue = &file->GatewayUsageMethod;
	else if (_stricmp(name, key_int_gatewayprofileusagemethod) == 0)
		*outValue = &file->GatewayProfileUsageMethod;
	else if (_stricmp(name, key_int_gatewaycredentialssource) == 0)
		*outValue = &file->GatewayCredentialsSource;
	else if (_stricmp(name, key_int_use_redirection_server_name) == 0)
		*outValue = &file->UseRedirectionServerName;
	else if (_stricmp(name, key_int_rdgiskdcproxy) == 0)
		*outValue = &file->RdgIsKdcProxy;
	else if (_stricmp(name, key_int_redirectwebauthn) == 0)
		*outValue = &file->RedirectWebauthN;
	else
	{
		rdpFileLine* line = freerdp_client_rdp_file_find_line_by_name(file, name);
		if (!line)
			return FALSE;
		if (!(line->flags & RDP_FILE_LINE_FLAG_TYPE_INTEGER))
			return FALSE;

		*outLine = line;
	}

	return TRUE;
}

static BOOL freerdp_client_rdp_file_find_string_entry(rdpFile* file, const char* name,
                                                      LPSTR** outValue, rdpFileLine** outLine)
{
	WINPR_ASSERT(file);
	WINPR_ASSERT(name);
	WINPR_ASSERT(outValue);
	WINPR_ASSERT(outLine);

	*outValue = NULL;
	*outLine = NULL;

	if (_stricmp(name, key_str_username) == 0)
		*outValue = &file->Username;
	else if (_stricmp(name, key_str_domain) == 0)
		*outValue = &file->Domain;
	else if (_stricmp(name, key_str_password) == 0)
		*outValue = &file->Password;
	else if (_stricmp(name, key_str_full_address) == 0)
		*outValue = &file->FullAddress;
	else if (_stricmp(name, key_str_alternate_full_address) == 0)
		*outValue = &file->AlternateFullAddress;
	else if (_stricmp(name, key_str_usbdevicestoredirect) == 0)
		*outValue = &file->UsbDevicesToRedirect;
	else if (_stricmp(name, key_str_camerastoredirect) == 0)
		*outValue = &file->RedirectCameras;
	else if (_stricmp(name, key_str_loadbalanceinfo) == 0)
		*outValue = &file->LoadBalanceInfo;
	else if (_stricmp(name, key_str_remoteapplicationname) == 0)
		*outValue = &file->RemoteApplicationName;
	else if (_stricmp(name, key_str_remoteapplicationicon) == 0)
		*outValue = &file->RemoteApplicationIcon;
	else if (_stricmp(name, key_str_remoteapplicationprogram) == 0)
		*outValue = &file->RemoteApplicationProgram;
	else if (_stricmp(name, key_str_remoteapplicationfile) == 0)
		*outValue = &file->RemoteApplicationFile;
	else if (_stricmp(name, key_str_remoteapplicationguid) == 0)
		*outValue = &file->RemoteApplicationGuid;
	else if (_stricmp(name, key_str_remoteapplicationcmdline) == 0)
		*outValue = &file->RemoteApplicationCmdLine;
	else if (_stricmp(name, key_str_alternate_shell) == 0)
		*outValue = &file->AlternateShell;
	else if (_stricmp(name, key_str_shell_working_directory) == 0)
		*outValue = &file->ShellWorkingDirectory;
	else if (_stricmp(name, key_str_gatewayhostname) == 0)
		*outValue = &file->GatewayHostname;
	else if (_stricmp(name, key_str_resourceprovider) == 0)
		*outValue = &file->ResourceProvider;
	else if (_stricmp(name, key_str_wvd) == 0)
		*outValue = &file->WvdEndpointPool;
	else if (_stricmp(name, key_str_geo) == 0)
		*outValue = &file->geo;
	else if (_stricmp(name, key_str_armpath) == 0)
		*outValue = &file->armpath;
	else if (_stricmp(name, key_str_aadtenantid) == 0)
		*outValue = &file->aadtenantid;
	else if (_stricmp(name, key_str_diagnosticserviceurl) == 0)
		*outValue = &file->diagnosticserviceurl;
	else if (_stricmp(name, key_str_hubdiscoverygeourl) == 0)
		*outValue = &file->hubdiscoverygeourl;
	else if (_stricmp(name, key_str_activityhint) == 0)
		*outValue = &file->activityhint;
	else if (_stricmp(name, key_str_gatewayaccesstoken) == 0)
		*outValue = &file->GatewayAccessToken;
	else if (_stricmp(name, key_str_kdcproxyname) == 0)
		*outValue = &file->KdcProxyName;
	else if (_stricmp(name, key_str_drivestoredirect) == 0)
		*outValue = &file->DrivesToRedirect;
	else if (_stricmp(name, key_str_devicestoredirect) == 0)
		*outValue = &file->DevicesToRedirect;
	else if (_stricmp(name, key_str_winposstr) == 0)
		*outValue = &file->WinPosStr;
	else if (_stricmp(name, key_str_pcb) == 0)
		*outValue = &file->PreconnectionBlob;
	else if (_stricmp(name, key_str_selectedmonitors) == 0)
		*outValue = &file->SelectedMonitors;
	else
	{
		rdpFileLine* line = freerdp_client_rdp_file_find_line_by_name(file, name);
		if (!line)
			return FALSE;
		if (!(line->flags & RDP_FILE_LINE_FLAG_TYPE_STRING))
			return FALSE;

		*outLine = line;
	}

	return TRUE;
}

/*
 * Set an integer in a rdpFile
 *
 * @return FALSE if a standard name was set, TRUE for a non-standard name, FALSE on error
 *
 */
static BOOL freerdp_client_rdp_file_set_integer(rdpFile* file, const char* name, long value)
{
	DWORD* targetValue = NULL;
	rdpFileLine* line = NULL;
#ifdef DEBUG_CLIENT_FILE
	WLog_DBG(TAG, "%s:i:%ld", name, value);
#endif

	if (value < 0)
		return FALSE;

	if (!freerdp_client_rdp_file_find_integer_entry(file, name, &targetValue, &line))
	{
		SSIZE_T index = freerdp_client_rdp_file_add_line(file);
		if (index == -1)
			return FALSE;
		line = &file->lines[index];
	}

	if (targetValue)
	{
		*targetValue = (DWORD)value;
		return TRUE;
	}

	if (line)
	{
		free(line->name);
		line->name = _strdup(name);
		if (!line->name)
		{
			free(line->name);
			line->name = NULL;
			return FALSE;
		}

		line->iValue = value;
		line->flags = RDP_FILE_LINE_FLAG_FORMATTED;
		line->flags |= RDP_FILE_LINE_FLAG_TYPE_INTEGER;
		line->valueLength = 0;
		return TRUE;
	}

	return FALSE;
}

static BOOL freerdp_client_parse_rdp_file_integer(rdpFile* file, const char* name,
                                                  const char* value)
{
	char* endptr = NULL;
	long ivalue = 0;
	errno = 0;
	ivalue = strtol(value, &endptr, 0);

	if ((endptr == NULL) || (errno != 0) || (endptr == value) || (ivalue > INT32_MAX) ||
	    (ivalue < INT32_MIN))
	{
		if (file->flags & RDP_FILE_FLAG_PARSE_INT_RELAXED)
		{
			WLog_WARN(TAG, "Integer option %s has invalid value %s, using default", name, value);
			return TRUE;
		}
		else
		{
			WLog_ERR(TAG, "Failed to convert RDP file integer option %s [value=%s]", name, value);
			return FALSE;
		}
	}

	return freerdp_client_rdp_file_set_integer(file, name, ivalue);
}

/** set a string value in the provided rdp file context
 *
 * @param file rdpFile
 * @param name name of the string
 * @param value value of the string to set
 * @return 0 on success, 1 if the key wasn't found (not a standard key), -1 on error
 */

static BOOL freerdp_client_rdp_file_set_string(rdpFile* file, const char* name, const char* value)
{
	LPSTR* targetValue = NULL;
	rdpFileLine* line = NULL;
#ifdef DEBUG_CLIENT_FILE
	WLog_DBG(TAG, "%s:s:%s", name, value);
#endif

	if (!name || !value)
		return FALSE;

	if (!freerdp_client_rdp_file_find_string_entry(file, name, &targetValue, &line))
	{
		SSIZE_T index = freerdp_client_rdp_file_add_line(file);
		if (index == -1)
			return FALSE;
		line = &file->lines[index];
	}

	if (targetValue)
	{
		*targetValue = _strdup(value);
		if (!(*targetValue))
			return FALSE;
		return TRUE;
	}

	if (line)
	{
		free(line->name);
		free(line->sValue);
		line->name = _strdup(name);
		line->sValue = _strdup(value);
		if (!line->name || !line->sValue)
		{
			free(line->name);
			free(line->sValue);
			line->name = NULL;
			line->sValue = NULL;
			return FALSE;
		}

		line->flags = RDP_FILE_LINE_FLAG_FORMATTED;
		line->flags |= RDP_FILE_LINE_FLAG_TYPE_STRING;
		line->valueLength = 0;
		return TRUE;
	}

	return FALSE;
}

static BOOL freerdp_client_add_option(rdpFile* file, const char* option)
{
	return freerdp_addin_argv_add_argument(file->args, option);
}

static SSIZE_T freerdp_client_rdp_file_add_line(rdpFile* file)
{
	SSIZE_T index = (SSIZE_T)file->lineCount;

	while ((file->lineCount + 1) > file->lineSize)
	{
		size_t new_size = 0;
		rdpFileLine* new_line = NULL;
		new_size = file->lineSize * 2;
		new_line = (rdpFileLine*)realloc(file->lines, new_size * sizeof(rdpFileLine));

		if (!new_line)
			return -1;

		file->lines = new_line;
		file->lineSize = new_size;
	}

	ZeroMemory(&(file->lines[file->lineCount]), sizeof(rdpFileLine));
	file->lines[file->lineCount].index = (size_t)index;
	(file->lineCount)++;
	return index;
}

static BOOL freerdp_client_parse_rdp_file_string(rdpFile* file, char* name, char* value)
{
	return freerdp_client_rdp_file_set_string(file, name, value);
}

static BOOL freerdp_client_parse_rdp_file_option(rdpFile* file, const char* option)
{
	return freerdp_client_add_option(file, option);
}

BOOL freerdp_client_parse_rdp_file_buffer(rdpFile* file, const BYTE* buffer, size_t size)
{
	return freerdp_client_parse_rdp_file_buffer_ex(file, buffer, size, NULL);
}

static BOOL trim(char** strptr)
{
	char* start = NULL;
	char* str = NULL;
	char* end = NULL;

	start = str = *strptr;
	if (!str)
		return TRUE;
	if (!(~((size_t)str)))
		return TRUE;
	end = str + strlen(str) - 1;

	while (isspace(*str))
		str++;

	while ((end > str) && isspace(*end))
		end--;
	end[1] = '\0';
	if (start == str)
		*strptr = str;
	else
	{
		*strptr = _strdup(str);
		free(start);
		return *strptr != NULL;
	}

	return TRUE;
}

static BOOL trim_strings(rdpFile* file)
{
	if (!trim(&file->Username))
		return FALSE;
	if (!trim(&file->Domain))
		return FALSE;
	if (!trim(&file->AlternateFullAddress))
		return FALSE;
	if (!trim(&file->FullAddress))
		return FALSE;
	if (!trim(&file->UsbDevicesToRedirect))
		return FALSE;
	if (!trim(&file->RedirectCameras))
		return FALSE;
	if (!trim(&file->LoadBalanceInfo))
		return FALSE;
	if (!trim(&file->GatewayHostname))
		return FALSE;
	if (!trim(&file->GatewayAccessToken))
		return FALSE;
	if (!trim(&file->RemoteApplicationName))
		return FALSE;
	if (!trim(&file->RemoteApplicationIcon))
		return FALSE;
	if (!trim(&file->RemoteApplicationProgram))
		return FALSE;
	if (!trim(&file->RemoteApplicationFile))
		return FALSE;
	if (!trim(&file->RemoteApplicationGuid))
		return FALSE;
	if (!trim(&file->RemoteApplicationCmdLine))
		return FALSE;
	if (!trim(&file->AlternateShell))
		return FALSE;
	if (!trim(&file->ShellWorkingDirectory))
		return FALSE;
	if (!trim(&file->DrivesToRedirect))
		return FALSE;
	if (!trim(&file->DevicesToRedirect))
		return FALSE;
	if (!trim(&file->DevicesToRedirect))
		return FALSE;
	if (!trim(&file->WinPosStr))
		return FALSE;
	if (!trim(&file->PreconnectionBlob))
		return FALSE;
	if (!trim(&file->KdcProxyName))
		return FALSE;
	if (!trim(&file->SelectedMonitors))
		return FALSE;

	for (size_t i = 0; i < file->lineCount; ++i)
	{
		rdpFileLine* curLine = &file->lines[i];
		if (curLine->flags & RDP_FILE_LINE_FLAG_TYPE_STRING)
		{
			if (!trim(&curLine->sValue))
				return FALSE;
		}
	}

	return TRUE;
}

BOOL freerdp_client_parse_rdp_file_buffer_ex(rdpFile* file, const BYTE* buffer, size_t size,
                                             rdp_file_fkt_parse parse)
{
	BOOL rc = FALSE;
	size_t length = 0;
	char* line = NULL;
	char* type = NULL;
	char* context = NULL;
	char* d1 = NULL;
	char* d2 = NULL;
	char* beg = NULL;
	char* name = NULL;
	char* value = NULL;
	char* copy = NULL;

	if (!file)
		return FALSE;
	if (size < 2)
		return FALSE;

	if ((buffer[0] == BOM_UTF16_LE[0]) && (buffer[1] == BOM_UTF16_LE[1]))
	{
		LPCWSTR uc = (LPCWSTR)(&buffer[2]);
		size = size / sizeof(WCHAR) - 1;

		copy = ConvertWCharNToUtf8Alloc(uc, size, NULL);
		if (!copy)
		{
			WLog_ERR(TAG, "Failed to convert RDP file from UCS2 to UTF8");
			return FALSE;
		}
	}
	else
	{
		copy = calloc(1, size + sizeof(BYTE));

		if (!copy)
			return FALSE;

		memcpy(copy, buffer, size);
	}

	line = strtok_s(copy, "\r\n", &context);

	while (line)
	{
		length = strnlen(line, size);

		if (length > 1)
		{
			beg = line;
			if (beg[0] == '/')
			{
				if (!freerdp_client_parse_rdp_file_option(file, line))
					goto fail;

				goto next_line; /* FreeRDP option */
			}

			d1 = strchr(line, ':');

			if (!d1)
				goto next_line; /* not first delimiter */

			type = &d1[1];
			d2 = strchr(type, ':');

			if (!d2)
				goto next_line; /* no second delimiter */

			if ((d2 - d1) != 2)
				goto next_line; /* improper type length */

			*d1 = 0;
			*d2 = 0;
			name = beg;
			value = &d2[1];

			if (parse && parse(file->context, name, *type, value))
			{
			}
			else if (*type == 'i')
			{
				/* integer type */
				if (!freerdp_client_parse_rdp_file_integer(file, name, value))
					goto fail;
			}
			else if (*type == 's')
			{
				/* string type */
				if (!freerdp_client_parse_rdp_file_string(file, name, value))
					goto fail;
			}
			else if (*type == 'b')
			{
				/* binary type */
				WLog_ERR(TAG, "Unsupported RDP file binary option %s [value=%s]", name, value);
			}
		}

	next_line:
		line = strtok_s(NULL, "\r\n", &context);
	}

	rc = trim_strings(file);
fail:
	free(copy);
	return rc;
}

BOOL freerdp_client_parse_rdp_file(rdpFile* file, const char* name)
{
	return freerdp_client_parse_rdp_file_ex(file, name, NULL);
}

BOOL freerdp_client_parse_rdp_file_ex(rdpFile* file, const char* name, rdp_file_fkt_parse parse)
{
	BOOL status = 0;
	BYTE* buffer = NULL;
	FILE* fp = NULL;
	size_t read_size = 0;
	INT64 file_size = 0;
	const char* fname = name;

	if (!file || !name)
		return FALSE;

	if (_strnicmp(fname, "file://", 7) == 0)
		fname = &name[7];

	fp = winpr_fopen(fname, "r");
	if (!fp)
	{
		WLog_ERR(TAG, "Failed to open RDP file %s", name);
		return FALSE;
	}

	(void)_fseeki64(fp, 0, SEEK_END);
	file_size = _ftelli64(fp);
	(void)_fseeki64(fp, 0, SEEK_SET);

	if (file_size < 1)
	{
		WLog_ERR(TAG, "RDP file %s is empty", name);
		(void)fclose(fp);
		return FALSE;
	}

	buffer = (BYTE*)malloc((size_t)file_size + 2);

	if (!buffer)
	{
		(void)fclose(fp);
		return FALSE;
	}

	read_size = fread(buffer, (size_t)file_size, 1, fp);

	if (!read_size)
	{
		if (!ferror(fp))
			read_size = (size_t)file_size;
	}

	(void)fclose(fp);

	if (read_size < 1)
	{
		WLog_ERR(TAG, "Could not read from RDP file %s", name);
		free(buffer);
		return FALSE;
	}

	buffer[file_size] = '\0';
	buffer[file_size + 1] = '\0';
	status = freerdp_client_parse_rdp_file_buffer_ex(file, buffer, (size_t)file_size, parse);
	free(buffer);
	return status;
}

static INLINE BOOL FILE_POPULATE_STRING(char** _target, const rdpSettings* _settings,
                                        FreeRDP_Settings_Keys_String _option)
{
	WINPR_ASSERT(_target);
	WINPR_ASSERT(_settings);

	const char* str = freerdp_settings_get_string(_settings, _option);
	freerdp_client_file_string_check_free(*_target);
	*_target = (void*)~((size_t)NULL);
	if (str)
	{
		*_target = _strdup(str);
		if (!_target)
			return FALSE;
	}
	return TRUE;
}

static char* freerdp_client_channel_args_to_string(const rdpSettings* settings, const char* channel,
                                                   const char* option)
{
	ADDIN_ARGV* args = freerdp_dynamic_channel_collection_find(settings, channel);
	const char* filters[] = { option };
	if (!args || (args->argc < 2))
		return NULL;

	return CommandLineToCommaSeparatedValuesEx(args->argc - 1, args->argv + 1, filters,
	                                           ARRAYSIZE(filters));
}

static BOOL rdp_opt_duplicate(const rdpSettings* _settings, FreeRDP_Settings_Keys_String _id,
                              char** _key)
{
	WINPR_ASSERT(_settings);
	WINPR_ASSERT(_key);
	const char* tmp = freerdp_settings_get_string(_settings, _id);

	if (tmp)
	{
		*_key = _strdup(tmp);
		if (!*_key)
			return FALSE;
	}

	return TRUE;
}

BOOL freerdp_client_populate_rdp_file_from_settings(rdpFile* file, const rdpSettings* settings)
{
	FreeRDP_Settings_Keys_String index = FreeRDP_STRING_UNUSED;
	UINT32 LoadBalanceInfoLength = 0;
	const char* GatewayHostname = NULL;
	char* redirectCameras = NULL;

	if (!file || !settings)
		return FALSE;

	if (!FILE_POPULATE_STRING(&file->Domain, settings, FreeRDP_Domain) ||
	    !FILE_POPULATE_STRING(&file->Username, settings, FreeRDP_Username) ||
	    !FILE_POPULATE_STRING(&file->Password, settings, FreeRDP_Password) ||
	    !FILE_POPULATE_STRING(&file->FullAddress, settings, FreeRDP_ServerHostname) ||
	    !FILE_POPULATE_STRING(&file->AlternateFullAddress, settings, FreeRDP_ServerHostname) ||
	    !FILE_POPULATE_STRING(&file->AlternateShell, settings, FreeRDP_AlternateShell) ||
	    !FILE_POPULATE_STRING(&file->DrivesToRedirect, settings, FreeRDP_DrivesToRedirect))

		return FALSE;
	file->ServerPort = freerdp_settings_get_uint32(settings, FreeRDP_ServerPort);

	file->DesktopWidth = freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth);
	file->DesktopHeight = freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight);
	file->SessionBpp = freerdp_settings_get_uint32(settings, FreeRDP_ColorDepth);
	file->DesktopScaleFactor = freerdp_settings_get_uint32(settings, FreeRDP_DesktopScaleFactor);
	file->DynamicResolution = freerdp_settings_get_bool(settings, FreeRDP_DynamicResolutionUpdate);
	file->VideoPlaybackMode = freerdp_settings_get_bool(settings, FreeRDP_SupportVideoOptimized);

	// TODO file->MaximizeToCurrentDisplays;
	// TODO file->SingleMonInWindowedMode;
	// TODO file->EncodeRedirectedVideoCapture;
	// TODO file->RedirectedVideoCaptureEncodingQuality;
	file->ConnectToConsole = freerdp_settings_get_bool(settings, FreeRDP_ConsoleSession);
	file->NegotiateSecurityLayer =
	    freerdp_settings_get_bool(settings, FreeRDP_NegotiateSecurityLayer);
	file->EnableCredSSPSupport = freerdp_settings_get_bool(settings, FreeRDP_NlaSecurity);
	file->EnableRdsAadAuth = freerdp_settings_get_bool(settings, FreeRDP_AadSecurity);

	if (freerdp_settings_get_bool(settings, FreeRDP_RemoteApplicationMode))
		index = FreeRDP_RemoteApplicationWorkingDir;
	else
		index = FreeRDP_ShellWorkingDirectory;
	if (!FILE_POPULATE_STRING(&file->ShellWorkingDirectory, settings, index))
		return FALSE;
	file->ConnectionType = freerdp_settings_get_uint32(settings, FreeRDP_ConnectionType);

	file->ScreenModeId = freerdp_settings_get_bool(settings, FreeRDP_Fullscreen) ? 2 : 1;

	LoadBalanceInfoLength = freerdp_settings_get_uint32(settings, FreeRDP_LoadBalanceInfoLength);
	if (LoadBalanceInfoLength > 0)
	{
		const BYTE* LoadBalanceInfo =
		    freerdp_settings_get_pointer(settings, FreeRDP_LoadBalanceInfo);
		file->LoadBalanceInfo = calloc(LoadBalanceInfoLength + 1, 1);
		if (!file->LoadBalanceInfo)
			return FALSE;
		memcpy(file->LoadBalanceInfo, LoadBalanceInfo, LoadBalanceInfoLength);
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_AudioPlayback))
		file->AudioMode = AUDIO_MODE_REDIRECT;
	else if (freerdp_settings_get_bool(settings, FreeRDP_RemoteConsoleAudio))
		file->AudioMode = AUDIO_MODE_PLAY_ON_SERVER;
	else
		file->AudioMode = AUDIO_MODE_NONE;

	/* The gateway hostname should also contain a port specifier unless it is the default port 443
	 */
	GatewayHostname = freerdp_settings_get_string(settings, FreeRDP_GatewayHostname);
	if (GatewayHostname)
	{
		const UINT32 GatewayPort = freerdp_settings_get_uint32(settings, FreeRDP_GatewayPort);
		freerdp_client_file_string_check_free(file->GatewayHostname);
		if (GatewayPort == 443)
			file->GatewayHostname = _strdup(GatewayHostname);
		else
		{
			int length = _scprintf("%s:%" PRIu32, GatewayHostname, GatewayPort);
			if (length < 0)
				return FALSE;

			file->GatewayHostname = (char*)malloc((size_t)length + 1);
			if (!file->GatewayHostname)
				return FALSE;

			if (sprintf_s(file->GatewayHostname, (size_t)length + 1, "%s:%" PRIu32, GatewayHostname,
			              GatewayPort) < 0)
				return FALSE;
		}
		if (!file->GatewayHostname)
			return FALSE;
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_GatewayArmTransport))
		file->ResourceProvider = _strdup(str_resourceprovider_arm);

	if (!rdp_opt_duplicate(settings, FreeRDP_GatewayAvdWvdEndpointPool, &file->WvdEndpointPool))
		return FALSE;
	if (!rdp_opt_duplicate(settings, FreeRDP_GatewayAvdGeo, &file->geo))
		return FALSE;
	if (!rdp_opt_duplicate(settings, FreeRDP_GatewayAvdArmpath, &file->armpath))
		return FALSE;
	if (!rdp_opt_duplicate(settings, FreeRDP_GatewayAvdAadtenantid, &file->aadtenantid))
		return FALSE;
	if (!rdp_opt_duplicate(settings, FreeRDP_GatewayAvdDiagnosticserviceurl,
	                       &file->diagnosticserviceurl))
		return FALSE;
	if (!rdp_opt_duplicate(settings, FreeRDP_GatewayAvdHubdiscoverygeourl,
	                       &file->hubdiscoverygeourl))
		return FALSE;
	if (!rdp_opt_duplicate(settings, FreeRDP_GatewayAvdActivityhint, &file->activityhint))
		return FALSE;

	file->AudioCaptureMode = freerdp_settings_get_bool(settings, FreeRDP_AudioCapture);
	file->BitmapCachePersistEnable =
	    freerdp_settings_get_bool(settings, FreeRDP_BitmapCachePersistEnabled);
	file->Compression = freerdp_settings_get_bool(settings, FreeRDP_CompressionEnabled);
	file->AuthenticationLevel = freerdp_settings_get_uint32(settings, FreeRDP_AuthenticationLevel);
	file->GatewayUsageMethod = freerdp_settings_get_uint32(settings, FreeRDP_GatewayUsageMethod);
	file->GatewayCredentialsSource =
	    freerdp_settings_get_uint32(settings, FreeRDP_GatewayCredentialsSource);
	file->PromptCredentialOnce =
	    freerdp_settings_get_bool(settings, FreeRDP_GatewayUseSameCredentials);
	file->PromptForCredentials = freerdp_settings_get_bool(settings, FreeRDP_PromptForCredentials);
	file->RemoteApplicationMode =
	    freerdp_settings_get_bool(settings, FreeRDP_RemoteApplicationMode);
	if (!FILE_POPULATE_STRING(&file->GatewayAccessToken, settings, FreeRDP_GatewayAccessToken) ||
	    !FILE_POPULATE_STRING(&file->RemoteApplicationProgram, settings,
	                          FreeRDP_RemoteApplicationProgram) ||
	    !FILE_POPULATE_STRING(&file->RemoteApplicationName, settings,
	                          FreeRDP_RemoteApplicationName) ||
	    !FILE_POPULATE_STRING(&file->RemoteApplicationIcon, settings,
	                          FreeRDP_RemoteApplicationIcon) ||
	    !FILE_POPULATE_STRING(&file->RemoteApplicationFile, settings,
	                          FreeRDP_RemoteApplicationFile) ||
	    !FILE_POPULATE_STRING(&file->RemoteApplicationGuid, settings,
	                          FreeRDP_RemoteApplicationGuid) ||
	    !FILE_POPULATE_STRING(&file->RemoteApplicationCmdLine, settings,
	                          FreeRDP_RemoteApplicationCmdLine))
		return FALSE;
	file->SpanMonitors = freerdp_settings_get_bool(settings, FreeRDP_SpanMonitors);
	file->UseMultiMon = freerdp_settings_get_bool(settings, FreeRDP_UseMultimon);
	file->AllowDesktopComposition =
	    freerdp_settings_get_bool(settings, FreeRDP_AllowDesktopComposition);
	file->AllowFontSmoothing = freerdp_settings_get_bool(settings, FreeRDP_AllowFontSmoothing);
	file->DisableWallpaper = freerdp_settings_get_bool(settings, FreeRDP_DisableWallpaper);
	file->DisableFullWindowDrag =
	    freerdp_settings_get_bool(settings, FreeRDP_DisableFullWindowDrag);
	file->DisableMenuAnims = freerdp_settings_get_bool(settings, FreeRDP_DisableMenuAnims);
	file->DisableThemes = freerdp_settings_get_bool(settings, FreeRDP_DisableThemes);
	file->BandwidthAutoDetect = (freerdp_settings_get_uint32(settings, FreeRDP_ConnectionType) >=
	                             CONNECTION_TYPE_AUTODETECT)
	                                ? TRUE
	                                : FALSE;
	file->NetworkAutoDetect =
	    freerdp_settings_get_bool(settings, FreeRDP_NetworkAutoDetect) ? 1 : 0;
	file->AutoReconnectionEnabled =
	    freerdp_settings_get_bool(settings, FreeRDP_AutoReconnectionEnabled);
	file->RedirectSmartCards = freerdp_settings_get_bool(settings, FreeRDP_RedirectSmartCards);
	file->RedirectWebauthN = freerdp_settings_get_bool(settings, FreeRDP_RedirectWebAuthN);

	redirectCameras =
	    freerdp_client_channel_args_to_string(settings, RDPECAM_DVC_CHANNEL_NAME, "device:");
	if (redirectCameras)
	{
		char* str =
		    freerdp_client_channel_args_to_string(settings, RDPECAM_DVC_CHANNEL_NAME, "encode:");
		file->EncodeRedirectedVideoCapture = 0;
		if (str)
		{
			unsigned long val = 0;
			errno = 0;
			val = strtoul(str, NULL, 0);
			if ((val < UINT32_MAX) && (errno == 0))
				file->EncodeRedirectedVideoCapture = val;
		}
		free(str);

		str = freerdp_client_channel_args_to_string(settings, RDPECAM_DVC_CHANNEL_NAME, "quality:");
		file->RedirectedVideoCaptureEncodingQuality = 0;
		if (str)
		{
			unsigned long val = 0;
			errno = 0;
			val = strtoul(str, NULL, 0);
			if ((val <= 2) && (errno == 0))
			{
				file->RedirectedVideoCaptureEncodingQuality = val;
			}
		}
		free(str);

		file->RedirectCameras = redirectCameras;
	}
#ifdef CHANNEL_URBDRC_CLIENT
	char* redirectUsb =
	    freerdp_client_channel_args_to_string(settings, URBDRC_CHANNEL_NAME, "device:");
	if (redirectUsb)
		file->UsbDevicesToRedirect = redirectUsb;

#endif
	file->RedirectClipboard =
	    freerdp_settings_get_bool(settings, FreeRDP_RedirectClipboard) ? 1 : 0;
	file->RedirectPrinters = freerdp_settings_get_bool(settings, FreeRDP_RedirectPrinters) ? 1 : 0;
	file->RedirectDrives = freerdp_settings_get_bool(settings, FreeRDP_RedirectDrives) ? 1 : 0;
	file->RdgIsKdcProxy = freerdp_settings_get_bool(settings, FreeRDP_KerberosRdgIsProxy) ? 1 : 0;
	file->RedirectComPorts = (freerdp_settings_get_bool(settings, FreeRDP_RedirectSerialPorts) ||
	                          freerdp_settings_get_bool(settings, FreeRDP_RedirectParallelPorts));
	file->RedirectLocation =
	    freerdp_dynamic_channel_collection_find(settings, LOCATION_CHANNEL_NAME) ? TRUE : FALSE;
	if (!FILE_POPULATE_STRING(&file->DrivesToRedirect, settings, FreeRDP_DrivesToRedirect) ||
	    !FILE_POPULATE_STRING(&file->PreconnectionBlob, settings, FreeRDP_PreconnectionBlob) ||
	    !FILE_POPULATE_STRING(&file->KdcProxyName, settings, FreeRDP_KerberosKdcUrl))
		return FALSE;

	{
		size_t offset = 0;
		UINT32 count = freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorIds);
		const UINT32* MonitorIds = freerdp_settings_get_pointer(settings, FreeRDP_MonitorIds);
		/* String size: 10 char UINT32 max string length, 1 char separator, one element NULL */
		size_t size = count * (10 + 1) + 1;

		char* str = calloc(size, sizeof(char));
		for (UINT32 x = 0; x < count; x++)
		{
			int rc = _snprintf(&str[offset], size - offset, "%" PRIu32 ",", MonitorIds[x]);
			if (rc <= 0)
			{
				free(str);
				return FALSE;
			}
			offset += (size_t)rc;
		}
		if (offset > 0)
			str[offset - 1] = '\0';
		freerdp_client_file_string_check_free(file->SelectedMonitors);
		file->SelectedMonitors = str;
	}

	file->KeyboardHook = freerdp_settings_get_uint32(settings, FreeRDP_KeyboardHook);

	return TRUE;
}

BOOL freerdp_client_write_rdp_file(const rdpFile* file, const char* name, BOOL unicode)
{
	int status = 0;
	WCHAR* unicodestr = NULL;

	if (!file || !name)
		return FALSE;

	const size_t size = freerdp_client_write_rdp_file_buffer(file, NULL, 0);
	if (size == 0)
		return FALSE;
	char* buffer = calloc(size + 1ULL, sizeof(char));

	if (freerdp_client_write_rdp_file_buffer(file, buffer, size + 1) != size)
	{
		WLog_ERR(TAG, "freerdp_client_write_rdp_file: error writing to output buffer");
		free(buffer);
		return FALSE;
	}

	FILE* fp = winpr_fopen(name, "w+b");

	if (fp)
	{
		if (unicode)
		{
			size_t len = 0;
			unicodestr = ConvertUtf8NToWCharAlloc(buffer, size, &len);

			if (!unicodestr)
			{
				free(buffer);
				(void)fclose(fp);
				return FALSE;
			}

			/* Write multi-byte header */
			if ((fwrite(BOM_UTF16_LE, sizeof(BYTE), 2, fp) != 2) ||
			    (fwrite(unicodestr, sizeof(WCHAR), len, fp) != len))
			{
				free(buffer);
				free(unicodestr);
				(void)fclose(fp);
				return FALSE;
			}

			free(unicodestr);
		}
		else
		{
			if (fwrite(buffer, 1, size, fp) != size)
			{
				free(buffer);
				(void)fclose(fp);
				return FALSE;
			}
		}

		(void)fflush(fp);
		status = fclose(fp);
	}

	free(buffer);
	return (status == 0) ? TRUE : FALSE;
}

WINPR_ATTR_FORMAT_ARG(3, 4)
static SSIZE_T freerdp_client_write_setting_to_buffer(char** buffer, size_t* bufferSize,
                                                      WINPR_FORMAT_ARG const char* fmt, ...)
{
	va_list ap = { 0 };
	SSIZE_T len = 0;
	char* buf = NULL;
	size_t bufSize = 0;

	if (!buffer || !bufferSize || !fmt)
		return -1;

	buf = *buffer;
	bufSize = *bufferSize;

	va_start(ap, fmt);
	len = vsnprintf(buf, bufSize, fmt, ap);
	va_end(ap);
	if (len < 0)
		return -1;

	/* _snprintf doesn't add the ending \0 to its return value */
	++len;

	/* we just want to know the size - return it */
	if (!buf && !bufSize)
		return len;

	if (!buf)
		return -1;

	/* update buffer size and buffer position and replace \0 with \n */
	if (bufSize >= (size_t)len)
	{
		*bufferSize -= (size_t)len;
		buf[len - 1] = '\n';
		*buffer = buf + len;
	}
	else
		return -1;

	return len;
}

size_t freerdp_client_write_rdp_file_buffer(const rdpFile* file, char* buffer, size_t size)
{
	size_t totalSize = 0;

	if (!file)
		return 0;

	/* either buffer and size are null or non-null */
	if ((!buffer || !size) && (buffer || size))
		return 0;

#define WRITE_SETTING_(fmt_, ...)                                                                \
	{                                                                                            \
		SSIZE_T res = freerdp_client_write_setting_to_buffer(&buffer, &size, fmt_, __VA_ARGS__); \
		if (res < 0)                                                                             \
			return 0;                                                                            \
		totalSize += (size_t)res;                                                                \
	}

#define WRITE_SETTING_INT(key_, param_)                   \
	do                                                    \
	{                                                     \
		if (~(param_))                                    \
			WRITE_SETTING_("%s:i:%" PRIu32, key_, param_) \
	} while (0)

#define WRITE_SETTING_STR(key_, param_)             \
	do                                              \
	{                                               \
		if (~(size_t)(param_))                      \
			WRITE_SETTING_("%s:s:%s", key_, param_) \
	} while (0)

	/* integer parameters */
	WRITE_SETTING_INT(key_int_use_multimon, file->UseMultiMon);
	WRITE_SETTING_INT(key_int_maximizetocurrentdisplays, file->MaximizeToCurrentDisplays);
	WRITE_SETTING_INT(key_int_singlemoninwindowedmode, file->SingleMonInWindowedMode);
	WRITE_SETTING_INT(key_int_screen_mode_id, file->ScreenModeId);
	WRITE_SETTING_INT(key_int_span_monitors, file->SpanMonitors);
	WRITE_SETTING_INT(key_int_smart_sizing, file->SmartSizing);
	WRITE_SETTING_INT(key_int_dynamic_resolution, file->DynamicResolution);
	WRITE_SETTING_INT(key_int_enablesuperpan, file->EnableSuperSpan);
	WRITE_SETTING_INT(key_int_superpanaccelerationfactor, file->SuperSpanAccelerationFactor);
	WRITE_SETTING_INT(key_int_desktopwidth, file->DesktopWidth);
	WRITE_SETTING_INT(key_int_desktopheight, file->DesktopHeight);
	WRITE_SETTING_INT(key_int_desktop_size_id, file->DesktopSizeId);
	WRITE_SETTING_INT(key_int_session_bpp, file->SessionBpp);
	WRITE_SETTING_INT(key_int_desktopscalefactor, file->DesktopScaleFactor);
	WRITE_SETTING_INT(key_int_compression, file->Compression);
	WRITE_SETTING_INT(key_int_keyboardhook, file->KeyboardHook);
	WRITE_SETTING_INT(key_int_disable_ctrl_alt_del, file->DisableCtrlAltDel);
	WRITE_SETTING_INT(key_int_audiomode, file->AudioMode);
	WRITE_SETTING_INT(key_int_audioqualitymode, file->AudioQualityMode);
	WRITE_SETTING_INT(key_int_audiocapturemode, file->AudioCaptureMode);
	WRITE_SETTING_INT(key_int_encode_redirected_video_capture, file->EncodeRedirectedVideoCapture);
	WRITE_SETTING_INT(key_int_redirected_video_capture_encoding_quality,
	                  file->RedirectedVideoCaptureEncodingQuality);
	WRITE_SETTING_INT(key_int_videoplaybackmode, file->VideoPlaybackMode);
	WRITE_SETTING_INT(key_int_connection_type, file->ConnectionType);
	WRITE_SETTING_INT(key_int_networkautodetect, file->NetworkAutoDetect);
	WRITE_SETTING_INT(key_int_bandwidthautodetect, file->BandwidthAutoDetect);
	WRITE_SETTING_INT(key_int_pinconnectionbar, file->PinConnectionBar);
	WRITE_SETTING_INT(key_int_displayconnectionbar, file->DisplayConnectionBar);
	WRITE_SETTING_INT(key_int_workspaceid, file->WorkspaceId);
	WRITE_SETTING_INT(key_int_enableworkspacereconnect, file->EnableWorkspaceReconnect);
	WRITE_SETTING_INT(key_int_disable_wallpaper, file->DisableWallpaper);
	WRITE_SETTING_INT(key_int_allow_font_smoothing, file->AllowFontSmoothing);
	WRITE_SETTING_INT(key_int_allow_desktop_composition, file->AllowDesktopComposition);
	WRITE_SETTING_INT(key_int_disable_full_window_drag, file->DisableFullWindowDrag);
	WRITE_SETTING_INT(key_int_disable_menu_anims, file->DisableMenuAnims);
	WRITE_SETTING_INT(key_int_disable_themes, file->DisableThemes);
	WRITE_SETTING_INT(key_int_disable_cursor_setting, file->DisableCursorSetting);
	WRITE_SETTING_INT(key_int_bitmapcachesize, file->BitmapCacheSize);
	WRITE_SETTING_INT(key_int_bitmapcachepersistenable, file->BitmapCachePersistEnable);
	WRITE_SETTING_INT(key_int_server_port, file->ServerPort);
	WRITE_SETTING_INT(key_int_redirectdrives, file->RedirectDrives);
	WRITE_SETTING_INT(key_int_redirectprinters, file->RedirectPrinters);
	WRITE_SETTING_INT(key_int_redirectcomports, file->RedirectComPorts);
	WRITE_SETTING_INT(key_int_redirectlocation, file->RedirectLocation);
	WRITE_SETTING_INT(key_int_redirectsmartcards, file->RedirectSmartCards);
	WRITE_SETTING_INT(key_int_redirectclipboard, file->RedirectClipboard);
	WRITE_SETTING_INT(key_int_redirectposdevices, file->RedirectPosDevices);
	WRITE_SETTING_INT(key_int_redirectdirectx, file->RedirectDirectX);
	WRITE_SETTING_INT(key_int_disableprinterredirection, file->DisablePrinterRedirection);
	WRITE_SETTING_INT(key_int_disableclipboardredirection, file->DisableClipboardRedirection);
	WRITE_SETTING_INT(key_int_connect_to_console, file->ConnectToConsole);
	WRITE_SETTING_INT(key_int_administrative_session, file->AdministrativeSession);
	WRITE_SETTING_INT(key_int_autoreconnection_enabled, file->AutoReconnectionEnabled);
	WRITE_SETTING_INT(key_int_autoreconnect_max_retries, file->AutoReconnectMaxRetries);
	WRITE_SETTING_INT(key_int_public_mode, file->PublicMode);
	WRITE_SETTING_INT(key_int_authentication_level, file->AuthenticationLevel);
	WRITE_SETTING_INT(key_int_promptcredentialonce, file->PromptCredentialOnce);
	WRITE_SETTING_INT(key_int_prompt_for_credentials, file->PromptForCredentials);
	WRITE_SETTING_INT(key_int_negotiate_security_layer, file->NegotiateSecurityLayer);
	WRITE_SETTING_INT(key_int_enablecredsspsupport, file->EnableCredSSPSupport);
	WRITE_SETTING_INT(key_int_enablerdsaadauth, file->EnableRdsAadAuth);
	WRITE_SETTING_INT(key_int_remoteapplicationmode, file->RemoteApplicationMode);
	WRITE_SETTING_INT(key_int_remoteapplicationexpandcmdline, file->RemoteApplicationExpandCmdLine);
	WRITE_SETTING_INT(key_int_remoteapplicationexpandworkingdir,
	                  file->RemoteApplicationExpandWorkingDir);
	WRITE_SETTING_INT(key_int_disableconnectionsharing, file->DisableConnectionSharing);
	WRITE_SETTING_INT(key_int_disableremoteappcapscheck, file->DisableRemoteAppCapsCheck);
	WRITE_SETTING_INT(key_int_gatewayusagemethod, file->GatewayUsageMethod);
	WRITE_SETTING_INT(key_int_gatewayprofileusagemethod, file->GatewayProfileUsageMethod);
	WRITE_SETTING_INT(key_int_gatewaycredentialssource, file->GatewayCredentialsSource);
	WRITE_SETTING_INT(key_int_use_redirection_server_name, file->UseRedirectionServerName);
	WRITE_SETTING_INT(key_int_rdgiskdcproxy, file->RdgIsKdcProxy);
	WRITE_SETTING_INT(key_int_redirectwebauthn, file->RedirectWebauthN);

	/* string parameters */
	WRITE_SETTING_STR(key_str_username, file->Username);
	WRITE_SETTING_STR(key_str_domain, file->Domain);
	WRITE_SETTING_STR(key_str_password, file->Password);
	WRITE_SETTING_STR(key_str_full_address, file->FullAddress);
	WRITE_SETTING_STR(key_str_alternate_full_address, file->AlternateFullAddress);
	WRITE_SETTING_STR(key_str_usbdevicestoredirect, file->UsbDevicesToRedirect);
	WRITE_SETTING_STR(key_str_camerastoredirect, file->RedirectCameras);
	WRITE_SETTING_STR(key_str_loadbalanceinfo, file->LoadBalanceInfo);
	WRITE_SETTING_STR(key_str_remoteapplicationname, file->RemoteApplicationName);
	WRITE_SETTING_STR(key_str_remoteapplicationicon, file->RemoteApplicationIcon);
	WRITE_SETTING_STR(key_str_remoteapplicationprogram, file->RemoteApplicationProgram);
	WRITE_SETTING_STR(key_str_remoteapplicationfile, file->RemoteApplicationFile);
	WRITE_SETTING_STR(key_str_remoteapplicationguid, file->RemoteApplicationGuid);
	WRITE_SETTING_STR(key_str_remoteapplicationcmdline, file->RemoteApplicationCmdLine);
	WRITE_SETTING_STR(key_str_alternate_shell, file->AlternateShell);
	WRITE_SETTING_STR(key_str_shell_working_directory, file->ShellWorkingDirectory);
	WRITE_SETTING_STR(key_str_gatewayhostname, file->GatewayHostname);
	WRITE_SETTING_STR(key_str_resourceprovider, file->ResourceProvider);
	WRITE_SETTING_STR(key_str_wvd, file->WvdEndpointPool);
	WRITE_SETTING_STR(key_str_geo, file->geo);
	WRITE_SETTING_STR(key_str_armpath, file->armpath);
	WRITE_SETTING_STR(key_str_aadtenantid, file->aadtenantid);
	WRITE_SETTING_STR(key_str_diagnosticserviceurl, file->diagnosticserviceurl);
	WRITE_SETTING_STR(key_str_hubdiscoverygeourl, file->hubdiscoverygeourl);
	WRITE_SETTING_STR(key_str_activityhint, file->activityhint);
	WRITE_SETTING_STR(key_str_gatewayaccesstoken, file->GatewayAccessToken);
	WRITE_SETTING_STR(key_str_kdcproxyname, file->KdcProxyName);
	WRITE_SETTING_STR(key_str_drivestoredirect, file->DrivesToRedirect);
	WRITE_SETTING_STR(key_str_devicestoredirect, file->DevicesToRedirect);
	WRITE_SETTING_STR(key_str_winposstr, file->WinPosStr);
	WRITE_SETTING_STR(key_str_pcb, file->PreconnectionBlob);
	WRITE_SETTING_STR(key_str_selectedmonitors, file->SelectedMonitors);

	/* custom parameters */
	for (size_t i = 0; i < file->lineCount; ++i)
	{
		SSIZE_T res = -1;
		const rdpFileLine* curLine = &file->lines[i];

		if (curLine->flags & RDP_FILE_LINE_FLAG_TYPE_INTEGER)
			res = freerdp_client_write_setting_to_buffer(&buffer, &size, "%s:i:%" PRIu32,
			                                             curLine->name, (UINT32)curLine->iValue);
		else if (curLine->flags & RDP_FILE_LINE_FLAG_TYPE_STRING)
			res = freerdp_client_write_setting_to_buffer(&buffer, &size, "%s:s:%s", curLine->name,
			                                             curLine->sValue);
		if (res < 0)
			return 0;

		totalSize += (size_t)res;
	}

	return totalSize;
}

static ADDIN_ARGV* rdp_file_to_args(const char* channel, const char* values)
{
	size_t count = 0;
	char** p = NULL;
	ADDIN_ARGV* args = freerdp_addin_argv_new(0, NULL);
	if (!args)
		return NULL;
	if (!freerdp_addin_argv_add_argument(args, channel))
		goto fail;

	p = CommandLineParseCommaSeparatedValues(values, &count);
	for (size_t x = 0; x < count; x++)
	{
		BOOL rc = 0;
		const char* val = p[x];
		const size_t len = strlen(val) + 8;
		char* str = calloc(len, sizeof(char));
		if (!str)
			goto fail;

		(void)_snprintf(str, len, "device:%s", val);
		rc = freerdp_addin_argv_add_argument(args, str);
		free(str);
		if (!rc)
			goto fail;
	}
	free(p);
	return args;

fail:
	free(p);
	freerdp_addin_argv_free(args);
	return NULL;
}

BOOL freerdp_client_populate_settings_from_rdp_file(const rdpFile* file, rdpSettings* settings)
{
	BOOL setDefaultConnectionType = TRUE;

	if (!file || !settings)
		return FALSE;

	if (~((size_t)file->Domain))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_Domain, file->Domain))
			return FALSE;
	}

	if (~((size_t)file->Username))
	{
		char* user = NULL;
		char* domain = NULL;

		if (!freerdp_parse_username(file->Username, &user, &domain))
			return FALSE;

		if (!freerdp_settings_set_string(settings, FreeRDP_Username, user))
			return FALSE;

		if (!(~((size_t)file->Domain)) && domain)
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_Domain, domain))
				return FALSE;
		}

		free(user);
		free(domain);
	}

	if (~((size_t)file->Password))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_Password, file->Password))
			return FALSE;
	}

	{
		const char* address = NULL;

		/* With MSTSC alternate full address always wins,
		 * so mimic this. */
		if (~((size_t)file->AlternateFullAddress))
			address = file->AlternateFullAddress;
		else if (~((size_t)file->FullAddress))
			address = file->FullAddress;

		if (address)
		{
			int port = -1;
			char* host = NULL;

			if (!freerdp_parse_hostname(address, &host, &port))
				return FALSE;

			const BOOL rc = freerdp_settings_set_string(settings, FreeRDP_ServerHostname, host);
			free(host);
			if (!rc)
				return FALSE;

			if (port > 0)
			{
				if (!freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, (UINT32)port))
					return FALSE;
			}
		}
	}

	if (~file->ServerPort)
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, file->ServerPort))
			return FALSE;
	}

	if (~file->DesktopSizeId)
	{
		switch (file->DesktopSizeId)
		{
			case 0:
				if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, 640))
					return FALSE;
				if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, 480))
					return FALSE;
				break;
			case 1:
				if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, 800))
					return FALSE;
				if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, 600))
					return FALSE;
				break;
			case 2:
				if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, 1024))
					return FALSE;
				if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, 768))
					return FALSE;
				break;
			case 3:
				if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, 1280))
					return FALSE;
				if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, 1024))
					return FALSE;
				break;
			case 4:
				if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, 1600))
					return FALSE;
				if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, 1200))
					return FALSE;
				break;
			default:
				WLog_WARN(TAG, "Unsupported 'desktop size id' value %" PRIu32, file->DesktopSizeId);
				break;
		}
	}

	if (~file->DesktopWidth)
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, file->DesktopWidth))
			return FALSE;
	}

	if (~file->DesktopHeight)
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, file->DesktopHeight))
			return FALSE;
	}

	if (~file->SessionBpp)
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, file->SessionBpp))
			return FALSE;
	}

	if (~file->ConnectToConsole)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_ConsoleSession,
		                               file->ConnectToConsole != 0))
			return FALSE;
	}

	if (~file->AdministrativeSession)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_ConsoleSession,
		                               file->AdministrativeSession != 0))
			return FALSE;
	}

	if (~file->NegotiateSecurityLayer)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_NegotiateSecurityLayer,
		                               file->NegotiateSecurityLayer != 0))
			return FALSE;
	}

	if (~file->EnableCredSSPSupport)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity,
		                               file->EnableCredSSPSupport != 0))
			return FALSE;
	}

	if (~file->EnableRdsAadAuth)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_AadSecurity, file->EnableRdsAadAuth != 0))
			return FALSE;
	}

	if (~((size_t)file->AlternateShell))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_AlternateShell, file->AlternateShell))
			return FALSE;
	}

	if (~((size_t)file->ShellWorkingDirectory))
	{
		/* ShellWorkingDir is used for either, shell working dir or remote app working dir */
		FreeRDP_Settings_Keys_String targetId =
		    (~file->RemoteApplicationMode && file->RemoteApplicationMode != 0)
		        ? FreeRDP_RemoteApplicationWorkingDir
		        : FreeRDP_ShellWorkingDirectory;

		if (!freerdp_settings_set_string(settings, targetId, file->ShellWorkingDirectory))
			return FALSE;
	}

	if (~file->ScreenModeId)
	{
		/**
		 * Screen Mode Id:
		 * http://technet.microsoft.com/en-us/library/ff393692/
		 *
		 * This setting corresponds to the selection in the Display
		 * configuration slider on the Display tab under Options in RDC.
		 *
		 * Values:
		 *
		 * 1: The remote session will appear in a window.
		 * 2: The remote session will appear full screen.
		 */
		if (!freerdp_settings_set_bool(settings, FreeRDP_Fullscreen,
		                               (file->ScreenModeId == 2) ? TRUE : FALSE))
			return FALSE;
	}

	if (~(file->SmartSizing))
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_SmartSizing,
		                               (file->SmartSizing == 1) ? TRUE : FALSE))
			return FALSE;
		/**
		 *  SmartSizingWidth and SmartSizingHeight:
		 *
		 *  Adding this option to use the DesktopHeight	and DesktopWidth as
		 *  parameters for the SmartSizingWidth and SmartSizingHeight, as there
		 *  are no options for that in standard RDP files.
		 *
		 *  Equivalent of doing /smart-sizing:WxH
		 */
		if (((~(file->DesktopWidth) && ~(file->DesktopHeight)) || ~(file->DesktopSizeId)) &&
		    (file->SmartSizing == 1))
		{
			if (!freerdp_settings_set_uint32(settings, FreeRDP_SmartSizingWidth,
			                                 file->DesktopWidth))
				return FALSE;
			if (!freerdp_settings_set_uint32(settings, FreeRDP_SmartSizingHeight,
			                                 file->DesktopHeight))
				return FALSE;
		}
	}

	if (~((size_t)file->LoadBalanceInfo))
	{
		const size_t len = strlen(file->LoadBalanceInfo);
		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_LoadBalanceInfo,
		                                      file->LoadBalanceInfo, len))
			return FALSE;
	}

	if (~file->AuthenticationLevel)
	{
		/**
		 * Authentication Level:
		 * http://technet.microsoft.com/en-us/library/ff393709/
		 *
		 * This setting corresponds to the selection in the If server authentication
		 * fails drop-down list on the Advanced tab under Options in RDC.
		 *
		 * Values:
		 *
		 * 0: If server authentication fails, connect to the computer without warning (Connect and
		 * don’t warn me). 1: If server authentication fails, do not establish a connection (Do not
		 * connect). 2: If server authentication fails, show a warning and allow me to connect or
		 * refuse the connection (Warn me). 3: No authentication requirement is specified.
		 */
		if (!freerdp_settings_set_uint32(settings, FreeRDP_AuthenticationLevel,
		                                 file->AuthenticationLevel))
			return FALSE;
	}

	if (~file->ConnectionType)
	{
		if (!freerdp_set_connection_type(settings, file->ConnectionType))
			return FALSE;
		setDefaultConnectionType = FALSE;
	}

	if (~file->AudioMode)
	{
		switch (file->AudioMode)
		{
			case AUDIO_MODE_REDIRECT:
				if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteConsoleAudio, FALSE))
					return FALSE;
				if (!freerdp_settings_set_bool(settings, FreeRDP_AudioPlayback, TRUE))
					return FALSE;
				break;
			case AUDIO_MODE_PLAY_ON_SERVER:
				if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteConsoleAudio, TRUE))
					return FALSE;
				if (!freerdp_settings_set_bool(settings, FreeRDP_AudioPlayback, FALSE))
					return FALSE;
				break;
			case AUDIO_MODE_NONE:
			default:
				if (!freerdp_settings_set_bool(settings, FreeRDP_AudioPlayback, FALSE))
					return FALSE;
				if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteConsoleAudio, FALSE))
					return FALSE;
				break;
		}
	}

	if (~file->AudioCaptureMode)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_AudioCapture, file->AudioCaptureMode != 0))
			return FALSE;
	}

	if (~file->Compression)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_CompressionEnabled,
		                               file->Compression != 0))
			return FALSE;
	}

	if (~((size_t)file->GatewayHostname))
	{
		int port = -1;
		char* host = NULL;

		if (!freerdp_parse_hostname(file->GatewayHostname, &host, &port))
			return FALSE;

		const BOOL rc = freerdp_settings_set_string(settings, FreeRDP_GatewayHostname, host);
		free(host);
		if (!rc)
			return FALSE;

		if (port > 0)
		{
			if (!freerdp_settings_set_uint32(settings, FreeRDP_GatewayPort, (UINT32)port))
				return FALSE;
		}
	}

	if (~((size_t)file->ResourceProvider))
	{
		if (_stricmp(file->ResourceProvider, str_resourceprovider_arm) == 0)
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayArmTransport, TRUE))
				return FALSE;
		}
	}

	if (~((size_t)file->WvdEndpointPool))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_GatewayAvdWvdEndpointPool,
		                                 file->WvdEndpointPool))
			return FALSE;
	}

	if (~((size_t)file->geo))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_GatewayAvdGeo, file->geo))
			return FALSE;
	}

	if (~((size_t)file->armpath))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_GatewayAvdArmpath, file->armpath))
			return FALSE;
	}

	if (~((size_t)file->aadtenantid))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_GatewayAvdAadtenantid,
		                                 file->aadtenantid))
			return FALSE;
	}

	if (~((size_t)file->diagnosticserviceurl))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_GatewayAvdDiagnosticserviceurl,
		                                 file->diagnosticserviceurl))
			return FALSE;
	}

	if (~((size_t)file->hubdiscoverygeourl))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_GatewayAvdHubdiscoverygeourl,
		                                 file->hubdiscoverygeourl))
			return FALSE;
	}

	if (~((size_t)file->activityhint))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_GatewayAvdActivityhint,
		                                 file->activityhint))
			return FALSE;
	}

	if (~((size_t)file->GatewayAccessToken))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_GatewayAccessToken,
		                                 file->GatewayAccessToken))
			return FALSE;
	}

	if (~file->GatewayUsageMethod)
	{
		if (!freerdp_set_gateway_usage_method(settings, file->GatewayUsageMethod))
			return FALSE;
	}

	if (~file->PromptCredentialOnce)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayUseSameCredentials,
		                               file->PromptCredentialOnce != 0))
			return FALSE;
	}

	if (~file->PromptForCredentials)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_PromptForCredentials,
		                               file->PromptForCredentials != 0))
			return FALSE;
	}

	if (~file->RemoteApplicationMode)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteApplicationMode,
		                               file->RemoteApplicationMode != 0))
			return FALSE;
	}

	if (~((size_t)file->RemoteApplicationProgram))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationProgram,
		                                 file->RemoteApplicationProgram))
			return FALSE;
	}

	if (~((size_t)file->RemoteApplicationName))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationName,
		                                 file->RemoteApplicationName))
			return FALSE;
	}

	if (~((size_t)file->RemoteApplicationIcon))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationIcon,
		                                 file->RemoteApplicationIcon))
			return FALSE;
	}

	if (~((size_t)file->RemoteApplicationFile))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationFile,
		                                 file->RemoteApplicationFile))
			return FALSE;
	}

	if (~((size_t)file->RemoteApplicationGuid))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationGuid,
		                                 file->RemoteApplicationGuid))
			return FALSE;
	}

	if (~((size_t)file->RemoteApplicationCmdLine))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationCmdLine,
		                                 file->RemoteApplicationCmdLine))
			return FALSE;
	}

	if (~file->SpanMonitors)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_SpanMonitors, file->SpanMonitors != 0))
			return FALSE;
	}

	if (~file->UseMultiMon)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_UseMultimon, file->UseMultiMon != 0))
			return FALSE;
	}

	if (~file->AllowFontSmoothing)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_AllowFontSmoothing,
		                               file->AllowFontSmoothing != 0))
			return FALSE;
	}

	if (~file->DisableWallpaper)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_DisableWallpaper,
		                               file->DisableWallpaper != 0))
			return FALSE;
	}

	if (~file->DisableFullWindowDrag)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_DisableFullWindowDrag,
		                               file->DisableFullWindowDrag != 0))
			return FALSE;
	}

	if (~file->DisableMenuAnims)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_DisableMenuAnims,
		                               file->DisableMenuAnims != 0))
			return FALSE;
	}

	if (~file->DisableThemes)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_DisableThemes, file->DisableThemes != 0))
			return FALSE;
	}

	if (~file->AllowDesktopComposition)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_AllowDesktopComposition,
		                               file->AllowDesktopComposition != 0))
			return FALSE;
	}

	if (~file->BitmapCachePersistEnable)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_BitmapCachePersistEnabled,
		                               file->BitmapCachePersistEnable != 0))
			return FALSE;
	}

	if (~file->DisableRemoteAppCapsCheck)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_DisableRemoteAppCapsCheck,
		                               file->DisableRemoteAppCapsCheck != 0))
			return FALSE;
	}

	if (~file->BandwidthAutoDetect)
	{
		if (file->BandwidthAutoDetect != 0)
		{
			if ((~file->NetworkAutoDetect) && (file->NetworkAutoDetect == 0))
			{
				WLog_WARN(TAG,
				          "Got networkautodetect:i:%" PRIu32 " and bandwidthautodetect:i:%" PRIu32
				          ". Correcting to networkautodetect:i:1",
				          file->NetworkAutoDetect, file->BandwidthAutoDetect);
				WLog_WARN(TAG,
				          "Add networkautodetect:i:1 to your RDP file to eliminate this warning.");
			}

			if (!freerdp_set_connection_type(settings, CONNECTION_TYPE_AUTODETECT))
				return FALSE;
			setDefaultConnectionType = FALSE;
		}
		if (!freerdp_settings_set_bool(settings, FreeRDP_NetworkAutoDetect,
		                               (file->BandwidthAutoDetect != 0) ||
		                                   (file->NetworkAutoDetect != 0)))
			return FALSE;
	}

	if (~file->NetworkAutoDetect)
	{
		if (file->NetworkAutoDetect != 0)
		{
			if ((~file->BandwidthAutoDetect) && (file->BandwidthAutoDetect == 0))
			{
				WLog_WARN(TAG,
				          "Got networkautodetect:i:%" PRIu32 " and bandwidthautodetect:i:%" PRIu32
				          ". Correcting to bandwidthautodetect:i:1",
				          file->NetworkAutoDetect, file->BandwidthAutoDetect);
				WLog_WARN(
				    TAG, "Add bandwidthautodetect:i:1 to your RDP file to eliminate this warning.");
			}

			if (!freerdp_set_connection_type(settings, CONNECTION_TYPE_AUTODETECT))
				return FALSE;

			setDefaultConnectionType = FALSE;
		}
		if (!freerdp_settings_set_bool(settings, FreeRDP_NetworkAutoDetect,
		                               (file->BandwidthAutoDetect != 0) ||
		                                   (file->NetworkAutoDetect != 0)))
			return FALSE;
	}

	if (~file->AutoReconnectionEnabled)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_AutoReconnectionEnabled,
		                               file->AutoReconnectionEnabled != 0))
			return FALSE;
	}

	if (~file->AutoReconnectMaxRetries)
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_AutoReconnectMaxRetries,
		                                 file->AutoReconnectMaxRetries))
			return FALSE;
	}

	if (~file->RedirectSmartCards)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectSmartCards,
		                               file->RedirectSmartCards != 0))
			return FALSE;
	}

	if (~file->RedirectWebauthN)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectWebAuthN,
		                               file->RedirectWebauthN != 0))
			return FALSE;
	}

	if (~file->RedirectClipboard)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectClipboard,
		                               file->RedirectClipboard != 0))
			return FALSE;
	}

	if (~file->RedirectPrinters)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectPrinters,
		                               file->RedirectPrinters != 0))
			return FALSE;
	}

	if (~file->RedirectDrives)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectDrives, file->RedirectDrives != 0))
			return FALSE;
	}

	if (~file->RedirectPosDevices)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectSerialPorts,
		                               file->RedirectComPorts != 0) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_RedirectParallelPorts,
		                               file->RedirectComPorts != 0))
			return FALSE;
	}

	if (~file->RedirectComPorts)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectSerialPorts,
		                               file->RedirectComPorts != 0) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_RedirectParallelPorts,
		                               file->RedirectComPorts != 0))
			return FALSE;
	}

	if (~file->RedirectLocation && (file->RedirectLocation != 0))
	{
		size_t count = 0;
		union
		{
			void* pv;
			char** str;
			const char** cstr;
		} cnv;
		cnv.str = CommandLineParseCommaSeparatedValuesEx(LOCATION_CHANNEL_NAME, NULL, &count);
		const BOOL rc = freerdp_client_add_dynamic_channel(settings, count, cnv.cstr);
		free(cnv.pv);
		if (!rc)
			return FALSE;
	}

	if (~file->RedirectDirectX)
	{
		/* What is this?! */
	}

	if ((~((size_t)file->DevicesToRedirect)) && !utils_str_is_empty(file->DevicesToRedirect))
	{
		/**
		 * Devices to redirect:
		 * http://technet.microsoft.com/en-us/library/ff393728/
		 *
		 * This setting corresponds to the selections for Other supported Plug and Play
		 * (PnP) devices under More on the Local Resources tab under Options in RDC.
		 *
		 * Values:
		 *
		 * '*':
		 * 	Redirect all supported Plug and Play devices.
		 *
		 * 'DynamicDevices':
		 * 	Redirect any supported Plug and Play devices that are connected later.
		 *
		 * The hardware ID for the supported Plug and Play device:
		 * 	Redirect the specified supported Plug and Play device.
		 *
		 * Examples:
		 * 	devicestoredirect:s:*
		 * 	devicestoredirect:s:DynamicDevices
		 * 	devicestoredirect:s:USB\VID_04A9&PID_30C1\6&4BD985D&0&2;,DynamicDevices
		 *
		 */
		if (!freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection, TRUE))
			return FALSE;
	}

	if ((~((size_t)file->DrivesToRedirect)) && !utils_str_is_empty(file->DrivesToRedirect))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_DrivesToRedirect,
		                                 file->DrivesToRedirect))
			return FALSE;
	}

	if ((~((size_t)file->RedirectCameras)) && !utils_str_is_empty(file->RedirectCameras))
	{
#if defined(CHANNEL_RDPECAM_CLIENT)
		union
		{
			char** c;
			const char** cc;
		} cnv;
		ADDIN_ARGV* args = rdp_file_to_args(RDPECAM_DVC_CHANNEL_NAME, file->RedirectCameras);
		if (!args)
			return FALSE;

		BOOL status = TRUE;
		if (~file->EncodeRedirectedVideoCapture)
		{
			char encode[64] = { 0 };
			(void)_snprintf(encode, sizeof(encode), "encode:%" PRIu32,
			                file->EncodeRedirectedVideoCapture);
			if (!freerdp_addin_argv_add_argument(args, encode))
				status = FALSE;
		}
		if (~file->RedirectedVideoCaptureEncodingQuality)
		{
			char quality[64] = { 0 };
			(void)_snprintf(quality, sizeof(quality), "quality:%" PRIu32,
			                file->RedirectedVideoCaptureEncodingQuality);
			if (!freerdp_addin_argv_add_argument(args, quality))
				status = FALSE;
		}

		cnv.c = args->argv;
		if (status)
			status = freerdp_client_add_dynamic_channel(settings, args->argc, cnv.cc);
		freerdp_addin_argv_free(args);
		if (!status)
			return FALSE;
#else
		WLog_WARN(
		    TAG,
		    "This build does not support [MS-RDPECAM] camera redirection channel. Ignoring '%s'",
		    key_str_camerastoredirect);
#endif
	}

	if ((~((size_t)file->UsbDevicesToRedirect)) && !utils_str_is_empty(file->UsbDevicesToRedirect))
	{
#ifdef CHANNEL_URBDRC_CLIENT
		union
		{
			char** c;
			const char** cc;
		} cnv;
		ADDIN_ARGV* args = rdp_file_to_args(URBDRC_CHANNEL_NAME, file->UsbDevicesToRedirect);
		if (!args)
			return FALSE;
		cnv.c = args->argv;
		const BOOL status = freerdp_client_add_dynamic_channel(settings, args->argc, cnv.cc);
		freerdp_addin_argv_free(args);
		if (!status)
			return FALSE;
#else
		WLog_WARN(TAG,
		          "This build does not support [MS-RDPEUSB] usb redirection channel. Ignoring '%s'",
		          key_str_usbdevicestoredirect);
#endif
	}

	if (~file->KeyboardHook)
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardHook, file->KeyboardHook))
			return FALSE;
	}

	if (~(size_t)file->SelectedMonitors)
	{
		size_t count = 0;
		char** args = CommandLineParseCommaSeparatedValues(file->SelectedMonitors, &count);
		UINT32* list = NULL;

		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_MonitorIds, NULL, count))
		{
			free(args);
			return FALSE;
		}
		list = freerdp_settings_get_pointer_writable(settings, FreeRDP_MonitorIds);
		if (!list && (count > 0))
		{
			free(args);
			return FALSE;
		}
		for (size_t x = 0; x < count; x++)
		{
			unsigned long val = 0;
			errno = 0;
			val = strtoul(args[x], NULL, 0);
			if ((val >= UINT32_MAX) && (errno != 0))
			{
				free(args);
				free(list);
				return FALSE;
			}
			list[x] = val;
		}
		free(args);
	}

	if (~file->DynamicResolution)
	{
		const BOOL val = file->DynamicResolution != 0;
		if (val)
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_SupportDisplayControl, TRUE))
				return FALSE;
		}
		if (!freerdp_settings_set_bool(settings, FreeRDP_DynamicResolutionUpdate, val))
			return FALSE;
	}

	if (~file->DesktopScaleFactor)
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopScaleFactor,
		                                 file->DesktopScaleFactor))
			return FALSE;
	}

	if (~file->VideoPlaybackMode)
	{
		if (file->VideoPlaybackMode != 0)
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_SupportGeometryTracking, TRUE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_SupportVideoOptimized, TRUE))
				return FALSE;
		}
		else
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_SupportVideoOptimized, FALSE))
				return FALSE;
		}
	}
	// TODO file->MaximizeToCurrentDisplays;
	// TODO file->SingleMonInWindowedMode;
	// TODO file->EncodeRedirectedVideoCapture;
	// TODO file->RedirectedVideoCaptureEncodingQuality;

	if (~((size_t)file->PreconnectionBlob))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_PreconnectionBlob,
		                                 file->PreconnectionBlob) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_SendPreconnectionPdu, TRUE))
			return FALSE;
	}

	if (~((size_t)file->KdcProxyName))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_KerberosKdcUrl, file->KdcProxyName))
			return FALSE;
	}

	if (~((size_t)file->RdgIsKdcProxy))
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_KerberosRdgIsProxy,
		                               file->RdgIsKdcProxy != 0))
			return FALSE;
	}

	if (file->args->argc > 1)
	{
		WCHAR* ConnectionFile =
		    freerdp_settings_get_string_as_utf16(settings, FreeRDP_ConnectionFile, NULL);

		if (freerdp_client_settings_parse_command_line(settings, file->args->argc, file->args->argv,
		                                               FALSE) < 0)
		{
			free(ConnectionFile);
			return FALSE;
		}

		BOOL rc = freerdp_settings_set_string_from_utf16(settings, FreeRDP_ConnectionFile,
		                                                 ConnectionFile);
		free(ConnectionFile);
		if (!rc)
			return FALSE;
	}

	if (setDefaultConnectionType)
	{
		if (!freerdp_set_connection_type(settings, CONNECTION_TYPE_AUTODETECT))
			return FALSE;
	}

	return TRUE;
}

static rdpFileLine* freerdp_client_rdp_file_find_line_by_name(const rdpFile* file, const char* name)
{
	BOOL bFound = FALSE;
	rdpFileLine* line = NULL;

	for (size_t index = 0; index < file->lineCount; index++)
	{
		line = &(file->lines[index]);

		if (line->flags & RDP_FILE_LINE_FLAG_FORMATTED)
		{
			if (_stricmp(name, line->name) == 0)
			{
				bFound = TRUE;
				break;
			}
		}
	}

	return (bFound) ? line : NULL;
}
/**
 * Set a string option to a rdpFile
 * @param file rdpFile
 * @param name name of the option
 * @param value value of the option
 * @return 0 on success
 */
int freerdp_client_rdp_file_set_string_option(rdpFile* file, const char* name, const char* value)
{
	return freerdp_client_rdp_file_set_string(file, name, value);
}

const char* freerdp_client_rdp_file_get_string_option(const rdpFile* file, const char* name)
{
	LPSTR* value = NULL;
	rdpFileLine* line = NULL;

	rdpFile* wfile = WINPR_CAST_CONST_PTR_AWAY(file, rdpFile*);
	if (freerdp_client_rdp_file_find_string_entry(wfile, name, &value, &line))
	{
		if (value && ~(size_t)(*value))
			return *value;
		if (line)
			return line->sValue;
	}

	return NULL;
}

int freerdp_client_rdp_file_set_integer_option(rdpFile* file, const char* name, int value)
{
	return freerdp_client_rdp_file_set_integer(file, name, value);
}

int freerdp_client_rdp_file_get_integer_option(const rdpFile* file, const char* name)
{
	DWORD* value = NULL;
	rdpFileLine* line = NULL;

	rdpFile* wfile = WINPR_CAST_CONST_PTR_AWAY(file, rdpFile*);
	if (freerdp_client_rdp_file_find_integer_entry(wfile, name, &value, &line))
	{
		if (value && ~(*value))
			return *value;
		if (line)
			return (int)line->iValue;
	}

	return -1;
}

static void freerdp_client_file_string_check_free(LPSTR str)
{
	if (~((size_t)str))
		free(str);
}

rdpFile* freerdp_client_rdp_file_new(void)
{
	return freerdp_client_rdp_file_new_ex(0);
}

rdpFile* freerdp_client_rdp_file_new_ex(DWORD flags)
{
	rdpFile* file = (rdpFile*)calloc(1, sizeof(rdpFile));

	if (!file)
		return NULL;

	file->flags = flags;

	FillMemory(file, sizeof(rdpFile), 0xFF);
	file->lines = NULL;
	file->lineCount = 0;
	file->lineSize = 32;
	file->GatewayProfileUsageMethod = 1;
	file->lines = (rdpFileLine*)calloc(file->lineSize, sizeof(rdpFileLine));

	file->args = freerdp_addin_argv_new(0, NULL);
	if (!file->lines || !file->args)
		goto fail;

	if (!freerdp_client_add_option(file, "freerdp"))
		goto fail;

	return file;
fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	freerdp_client_rdp_file_free(file);
	WINPR_PRAGMA_DIAG_POP
	return NULL;
}
void freerdp_client_rdp_file_free(rdpFile* file)
{
	if (file)
	{
		if (file->lineCount)
		{
			for (size_t i = 0; i < file->lineCount; i++)
			{
				free(file->lines[i].name);
				free(file->lines[i].sValue);
			}
		}
		free(file->lines);

		freerdp_addin_argv_free(file->args);

		freerdp_client_file_string_check_free(file->Username);
		freerdp_client_file_string_check_free(file->Domain);
		freerdp_client_file_string_check_free(file->Password);
		freerdp_client_file_string_check_free(file->FullAddress);
		freerdp_client_file_string_check_free(file->AlternateFullAddress);
		freerdp_client_file_string_check_free(file->UsbDevicesToRedirect);
		freerdp_client_file_string_check_free(file->RedirectCameras);
		freerdp_client_file_string_check_free(file->SelectedMonitors);
		freerdp_client_file_string_check_free(file->LoadBalanceInfo);
		freerdp_client_file_string_check_free(file->RemoteApplicationName);
		freerdp_client_file_string_check_free(file->RemoteApplicationIcon);
		freerdp_client_file_string_check_free(file->RemoteApplicationProgram);
		freerdp_client_file_string_check_free(file->RemoteApplicationFile);
		freerdp_client_file_string_check_free(file->RemoteApplicationGuid);
		freerdp_client_file_string_check_free(file->RemoteApplicationCmdLine);
		freerdp_client_file_string_check_free(file->AlternateShell);
		freerdp_client_file_string_check_free(file->ShellWorkingDirectory);
		freerdp_client_file_string_check_free(file->GatewayHostname);
		freerdp_client_file_string_check_free(file->GatewayAccessToken);
		freerdp_client_file_string_check_free(file->KdcProxyName);
		freerdp_client_file_string_check_free(file->DrivesToRedirect);
		freerdp_client_file_string_check_free(file->DevicesToRedirect);
		freerdp_client_file_string_check_free(file->WinPosStr);
		freerdp_client_file_string_check_free(file->ResourceProvider);
		freerdp_client_file_string_check_free(file->WvdEndpointPool);
		freerdp_client_file_string_check_free(file->geo);
		freerdp_client_file_string_check_free(file->armpath);
		freerdp_client_file_string_check_free(file->aadtenantid);
		freerdp_client_file_string_check_free(file->diagnosticserviceurl);
		freerdp_client_file_string_check_free(file->hubdiscoverygeourl);
		freerdp_client_file_string_check_free(file->activityhint);
		free(file);
	}
}

void freerdp_client_rdp_file_set_callback_context(rdpFile* file, void* context)
{
	file->context = context;
}
