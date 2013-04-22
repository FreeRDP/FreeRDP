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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/client/file.h>
#include <freerdp/client/cmdline.h>

/**
 * Remote Desktop Plus - Overview of .rdp file settings:
 * http://www.donkz.nl/files/rdpsettings.html
 *
 * RDP Settings for Remote Desktop Services in Windows Server 2008 R2:
 * http://technet.microsoft.com/en-us/library/ff393699/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <winpr/crt.h>

#define DEBUG_CLIENT_FILE	1

static BYTE BOM_UTF16_LE[2] = { 0xFF, 0xFE };
static WCHAR CR_LF_STR_W[] = { '\r', '\n', '\0' };

#define INVALID_INTEGER_VALUE		0xFFFFFFFF

BOOL freerdp_client_rdp_file_set_integer(rdpFile* file, char* name, int value)
{
#ifdef DEBUG_CLIENT_FILE
	fprintf(stderr, "%s:i:%d\n", name, value);
#endif

	if (_stricmp(name, "use multimon") == 0)
		file->UseMultiMon = value;
	else if (_stricmp(name, "screen mode id") == 0)
		file->ScreenModeId = value;
	else if (_stricmp(name, "span monitors") == 0)
		file->SpanMonitors = value;
	else if (_stricmp(name, "smartsizing") == 0)
		file->SmartSizing = value;
	else if (_stricmp(name, "enablesuperpan") == 0)
		file->EnableSuperSpan = value;
	else if (_stricmp(name, "superpanaccelerationfactor") == 0)
		file->SuperSpanAccelerationFactor = value;
	else if (_stricmp(name, "desktopwidth") == 0)
		file->DesktopWidth = value;
	else if (_stricmp(name, "desktopheight") == 0)
		file->DesktopHeight = value;
	else if (_stricmp(name, "desktop size id") == 0)
		file->DesktopSizeId = value;
	else if (_stricmp(name, "session bpp") == 0)
		file->SessionBpp = value;
	else if (_stricmp(name, "compression") == 0)
		file->Compression = value;
	else if (_stricmp(name, "keyboardhook") == 0)
		file->KeyboardHook = value;
	else if (_stricmp(name, "disable ctrl+alt+del") == 0)
		file->DisableCtrlAltDel = value;
	else if (_stricmp(name, "audiomode") == 0)
		file->AudioMode = value;
	else if (_stricmp(name, "audioqualitymode") == 0)
		file->AudioQualityMode = value;
	else if (_stricmp(name, "audiocapturemode") == 0)
		file->AudioCaptureMode = value;
	else if (_stricmp(name, "videoplaybackmode") == 0)
		file->VideoPlaybackMode = value;
	else if (_stricmp(name, "connection type") == 0)
		file->ConnectionType = value;
	else if (_stricmp(name, "networkautodetect") == 0)
		file->NetworkAutoDetect = value;
	else if (_stricmp(name, "bandwidthautodetect") == 0)
		file->BandwidthAutoDetect = value;
	else if (_stricmp(name, "pinconnectionbar") == 0)
		file->PinConnectionBar = value;
	else if (_stricmp(name, "displayconnectionbar") == 0)
		file->DisplayConnectionBar = value;
	else if (_stricmp(name, "workspaceid") == 0)
		file->WorkspaceId = value;
	else if (_stricmp(name, "enableworkspacereconnect") == 0)
		file->EnableWorkspaceReconnect = value;
	else if (_stricmp(name, "disable wallpaper") == 0)
		file->DisableWallpaper = value;
	else if (_stricmp(name, "allow font smoothing") == 0)
		file->AllowFontSmoothing = value;
	else if (_stricmp(name, "allow desktop composition") == 0)
		file->AllowDesktopComposition = value;
	else if (_stricmp(name, "disable full window drag") == 0)
		file->DisableFullWindowDrag = value;
	else if (_stricmp(name, "disable menu anims") == 0)
		file->DisableMenuAnims = value;
	else if (_stricmp(name, "disable themes") == 0)
		file->DisableThemes = value;
	else if (_stricmp(name, "disable cursor setting") == 0)
		file->DisableCursorSetting = value;
	else if (_stricmp(name, "bitmapcachesize") == 0)
		file->BitmapCacheSize = value;
	else if (_stricmp(name, "bitmapcachepersistenable") == 0)
		file->BitmapCachePersistEnable = value;
	else if (_stricmp(name, "server port") == 0)
		file->ServerPort = value;
	else if (_stricmp(name, "redirectdrives") == 0)
		file->RedirectDrives = value;
	else if (_stricmp(name, "redirectprinters") == 0)
		file->RedirectPrinters = value;
	else if (_stricmp(name, "redirectcomports") == 0)
		file->RedirectComPorts = value;
	else if (_stricmp(name, "redirectsmartcards") == 0)
		file->RedirectSmartCards = value;
	else if (_stricmp(name, "redirectclipboard") == 0)
		file->RedirectClipboard = value;
	else if (_stricmp(name, "redirectposdevices") == 0)
		file->RedirectPosDevices = value;
	else if (_stricmp(name, "redirectdirectx") == 0)
		file->RedirectDirectX = value;
	else if (_stricmp(name, "disableprinterredirection") == 0)
		file->DisablePrinterRedirection = value;
	else if (_stricmp(name, "disableclipboardredirection") == 0)
		file->DisableClipboardRedirection = value;
	else if (_stricmp(name, "connect to console") == 0)
		file->ConnectToConsole = value;
	else if (_stricmp(name, "administrative session") == 0)
		file->AdministrativeSession = value;
	else if (_stricmp(name, "autoreconnection enabled") == 0)
		file->AutoReconnectionEnabled = value;
	else if (_stricmp(name, "autoreconnect max retries") == 0)
		file->AutoReconnectMaxRetries = value;
	else if (_stricmp(name, "public mode") == 0)
		file->PublicMode = value;
	else if (_stricmp(name, "authentication level") == 0)
		file->AuthenticationLevel = value;
	else if (_stricmp(name, "promptcredentialonce") == 0)
		file->PromptCredentialOnce = value;
	else if (_stricmp(name, "prompt for credentials") == 0)
		file->PromptForCredentials = value;
	else if (_stricmp(name, "promptcredentialonce") == 0)
		file->PromptForCredentialsOnce = value;
	else if (_stricmp(name, "negotiate security layer") == 0)
		file->NegotiateSecurityLayer = value;
	else if (_stricmp(name, "enablecredsspsupport") == 0)
		file->EnableCredSSPSupport = value;
	else if (_stricmp(name, "remoteapplicationmode") == 0)
		file->RemoteApplicationMode = value;
	else if (_stricmp(name, "remoteapplicationexpandcmdline") == 0)
		file->RemoteApplicationExpandCmdLine = value;
	else if (_stricmp(name, "remoteapplicationexpandworkingdir") == 0)
		file->RemoteApplicationExpandWorkingDir = value;
	else if (_stricmp(name, "disableconnectionsharing") == 0)
		file->DisableConnectionSharing = value;
	else if (_stricmp(name, "disableremoteappcapscheck") == 0)
		file->DisableRemoteAppCapsCheck = value;
	else if (_stricmp(name, "gatewayusagemethod") == 0)
		file->GatewayUsageMethod = value;
	else if (_stricmp(name, "gatewayprofileusagemethod") == 0)
		file->GatewayProfileUsageMethod = value;
	else if (_stricmp(name, "gatewaycredentialssource") == 0)
		file->GatewayCredentialsSource = value;
	else if (_stricmp(name, "use redirection server name") == 0)
		file->UseRedirectionServerName = value;
	else if (_stricmp(name, "rdgiskdcproxy") == 0)
		file->RdgIsKdcProxy = value;
	else
		return FALSE;

	return TRUE;
}

void freerdp_client_parse_rdp_file_integer_unicode(rdpFile* file, WCHAR* name, WCHAR* value)
{
	int length;
	int ivalue;
	char* nameA;
	char* valueA;

	length = _wcslen(name);
	nameA = (char*) malloc(length + 1);
	WideCharToMultiByte(CP_UTF8, 0, name, length, nameA, length, NULL, NULL);
	nameA[length] = '\0';

	length = _wcslen(value);
	valueA = (char*) malloc(length + 1);
	WideCharToMultiByte(CP_UTF8, 0, value, length, valueA, length, NULL, NULL);
	valueA[length] = '\0';

	ivalue = atoi(valueA);
	freerdp_client_rdp_file_set_integer(file, nameA, ivalue);

	free(nameA);
	free(valueA);
}

void freerdp_client_parse_rdp_file_integer_ascii(rdpFile* file, char* name, char* value)
{
	int ivalue = atoi(value);
	freerdp_client_rdp_file_set_integer(file, name, ivalue);
}

BOOL freerdp_client_rdp_file_set_string(rdpFile* file, char* name, char* value)
{
#ifdef DEBUG_CLIENT_FILE
	fprintf(stderr, "%s:s:%s\n", name, value);
#endif

	if (_stricmp(name, "username") == 0)
		file->Username = value;
	else if (_stricmp(name, "domain") == 0)
		file->Domain = value;
	else if (_stricmp(name, "full address") == 0)
		file->FullAddress = value;
	else if (_stricmp(name, "alternate full address") == 0)
		file->AlternateFullAddress = value;
	else if (_stricmp(name, "usbdevicestoredirect") == 0)
		file->UsbDevicesToRedirect = value;
	else if (_stricmp(name, "loadbalanceinfo") == 0)
		file->LoadBalanceInfo = value;
	else if (_stricmp(name, "remoteapplicationname") == 0)
		file->RemoteApplicationName = value;
	else if (_stricmp(name, "remoteapplicationicon") == 0)
		file->RemoteApplicationIcon = value;
	else if (_stricmp(name, "remoteapplicationprogram") == 0)
		file->RemoteApplicationProgram = value;
	else if (_stricmp(name, "remoteapplicationfile") == 0)
		file->RemoteApplicationFile = value;
	else if (_stricmp(name, "remoteapplicationguid") == 0)
		file->RemoteApplicationGuid = value;
	else if (_stricmp(name, "remoteapplicationcmdline") == 0)
		file->RemoteApplicationCmdLine = value;
	else if (_stricmp(name, "alternate shell") == 0)
		file->AlternateShell = value;
	else if (_stricmp(name, "shell working directory") == 0)
		file->ShellWorkingDirectory = value;
	else if (_stricmp(name, "gatewayhostname") == 0)
		file->GatewayHostname = value;
	else if (_stricmp(name, "kdcproxyname") == 0)
		file->KdcProxyName = value;
	else if (_stricmp(name, "drivestoredirect") == 0)
		file->DrivesToRedirect = value;
	else if (_stricmp(name, "devicestoredirect") == 0)
		file->DevicesToRedirect = value;
	else if (_stricmp(name, "winposstr") == 0)
		file->WinPosStr = value;
	else
		return FALSE;

	return TRUE;
}

void freerdp_client_parse_rdp_file_string_unicode(rdpFile* file, WCHAR* name, WCHAR* value)
{
	int length;
	char* nameA;
	char* valueA;

	length = _wcslen(name);
	nameA = (char*) malloc(length + 1);
	WideCharToMultiByte(CP_UTF8, 0, name, length, nameA, length, NULL, NULL);
	nameA[length] = '\0';

	length = _wcslen(value);
	valueA = (char*) malloc(length + 1);
	WideCharToMultiByte(CP_UTF8, 0, value, length, valueA, length, NULL, NULL);
	valueA[length] = '\0';

	if (!freerdp_client_rdp_file_set_string(file, nameA, valueA))
		free(valueA);

	free(nameA);
}

void freerdp_client_parse_rdp_file_string_ascii(rdpFile* file, char* name, char* value)
{
	freerdp_client_rdp_file_set_string(file, name, value);
}

BOOL freerdp_client_parse_rdp_file_buffer_ascii(rdpFile* file, BYTE* buffer, size_t size)
{
	int length;
	char* line;
	char* type;
	char* context;
	char *d1, *d2;
	char *beg, *end;
	char *name, *value;

	line = strtok_s((char*) buffer, "\r\n", &context);

	while (line != NULL)
	{
		length = strlen(line);

		if (length > 1)
		{
			beg = line;
			end = &line[length - 1];

			d1 = strchr(line, ':');

			if (!d1)
				goto next_line; /* not first delimiter */

			type = &d1[1];
			d2 = strchr(type, ':');

			if (!d2)
				goto next_line; /* no second delimiter */

			if ((d2 - d1) != 2)
				goto next_line; /* improper type length */

			if (d2 == end)
				goto next_line; /* no value */

			*d1 = 0;
			*d2 = 0;
			name = beg;
			value = &d2[1];

			if (*type == 'i')
			{
				/* integer type */
				freerdp_client_parse_rdp_file_integer_ascii(file, name, value);
			}
			else if (*type == 's')
			{
				/* string type */
				freerdp_client_parse_rdp_file_string_ascii(file, name, value);
			}
			else if (*type == 'b')
			{
				/* binary type */
			}
		}

next_line:
		line = strtok_s(NULL, "\r\n", &context);
	}

	return TRUE;
}

BOOL freerdp_client_parse_rdp_file_buffer_unicode(rdpFile* file, BYTE* buffer, size_t size)
{
	int length;
	WCHAR* line;
	WCHAR* type;
	WCHAR* context;
	WCHAR *d1, *d2;
	WCHAR *beg, *end;
	WCHAR *name, *value;

	line = wcstok_s((WCHAR*) buffer, CR_LF_STR_W, &context);

	while (line != NULL)
	{
		length = _wcslen(line);

		if (length > 1)
		{
			beg = line;
			end = &line[length - 1];

			d1 = _wcschr(line, ':');

			if (!d1)
				goto next_line; /* not first delimiter */

			type = &d1[1];
			d2 = _wcschr(type, ':');

			if (!d2)
				goto next_line; /* no second delimiter */

			if ((d2 - d1) != 2)
				goto next_line; /* improper type length */

			if (d2 == end)
				goto next_line; /* no value */

			*d1 = 0;
			*d2 = 0;
			name = beg;
			value = &d2[1];

			if (*type == 'i')
			{
				/* integer type */
				freerdp_client_parse_rdp_file_integer_unicode(file, name, value);
			}
			else if (*type == 's')
			{
				/* string type */
				freerdp_client_parse_rdp_file_string_unicode(file, name, value);
			}
			else if (*type == 'b')
			{
				/* binary type */
			}
		}

next_line:
		line = wcstok_s(NULL, CR_LF_STR_W, &context);
	}

	return TRUE;
}

BOOL freerdp_client_parse_rdp_file_buffer(rdpFile* file, BYTE* buffer, size_t size)
{
	if (size < 2)
		return FALSE;

	if ((buffer[0] == BOM_UTF16_LE[0]) && (buffer[1] == BOM_UTF16_LE[1]))
		return freerdp_client_parse_rdp_file_buffer_unicode(file, &buffer[2], size - 2);

	return freerdp_client_parse_rdp_file_buffer_ascii(file, buffer, size);
}

BOOL freerdp_client_parse_rdp_file(rdpFile* file, char* name)
{
	BYTE* buffer;
	FILE* fp = NULL;
	size_t read_size;
	long int file_size;

	fp = fopen(name, "r");

	if (!fp)
		return FALSE;

	fseek(fp, 0, SEEK_END);
	file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (file_size < 1)
		return FALSE;

	buffer = (BYTE*) malloc(file_size);

	read_size = fread(buffer, file_size, 1, fp);

	if (!read_size)
	{
		if (!ferror(fp))
			read_size = file_size;
	}

	if (read_size < 1)
	{
		free(buffer);
		buffer = NULL;
		return FALSE;
	}

	return freerdp_client_parse_rdp_file_buffer(file, buffer, file_size);
}

BOOL freerdp_client_populate_rdp_file_from_settings(rdpFile* file, rdpSettings* settings)
{
	if (settings->Domain)
		file->Domain = settings->Domain;

	if (settings->Username)
	{
		file->Username = settings->Username;
	}

	if (settings->ServerPort)
		file->ServerPort = settings->ServerPort;
	if (settings->ServerHostname)
		file->FullAddress = settings->ServerHostname;
	if (settings->DesktopWidth)
		file->DesktopWidth = settings->DesktopWidth;
	if (settings->DesktopHeight)
		file->DesktopHeight = settings->DesktopHeight;
	if (settings->ColorDepth)
		file->SessionBpp = settings->ColorDepth;
	if (settings->ConsoleSession)
		file->ConnectToConsole = settings->ConsoleSession;
	if (settings->ConsoleSession)
		file->AdministrativeSession = settings->ConsoleSession;
	if (settings->NegotiateSecurityLayer)
		file->NegotiateSecurityLayer = settings->NegotiateSecurityLayer;
	if (settings->NlaSecurity)
		file->EnableCredSSPSupport = settings->NlaSecurity;
	if (settings->AlternateShell)
		file->AlternateShell = settings->AlternateShell;
	if (settings->ShellWorkingDirectory)
		file->ShellWorkingDirectory = settings->ShellWorkingDirectory;
	
	file->ConnectionType = freerdp_get_connection_type(settings);

	if (settings->AudioPlayback)
	{
		file->AudioMode = AUDIO_MODE_REDIRECT;
	} 
	else if (settings->RemoteConsoleAudio)
	{
		file->AudioMode = AUDIO_MODE_PLAY_ON_SERVER;
	}
	else
	{
		file->AudioMode = AUDIO_MODE_NONE;
	}

	if (settings->GatewayHostname)
		file->GatewayHostname = settings->GatewayHostname;
	if (settings->GatewayUsageMethod)
		file->GatewayUsageMethod = TRUE;
	if (settings->GatewayUseSameCredentials)
		file->PromptCredentialOnce = TRUE;
	
	if (settings->RemoteApplicationMode)
		file->RemoteApplicationMode = settings->RemoteApplicationMode;
	if (settings->RemoteApplicationProgram)
		file->RemoteApplicationProgram = settings->RemoteApplicationProgram;
	if (settings->RemoteApplicationName)
		file->RemoteApplicationName = settings->RemoteApplicationName;
	if (settings->RemoteApplicationIcon)
		file->RemoteApplicationIcon = settings->RemoteApplicationIcon;
	if (settings->RemoteApplicationFile)
		file->RemoteApplicationFile = settings->RemoteApplicationFile;
	if (settings->RemoteApplicationGuid)
		file->RemoteApplicationGuid = settings->RemoteApplicationGuid;
	if (settings->RemoteApplicationCmdLine)
		file->RemoteApplicationCmdLine = settings->RemoteApplicationCmdLine;

	if (settings->SpanMonitors)
		file->SpanMonitors = settings->SpanMonitors;
	if (settings->UseMultimon)
		file->UseMultiMon = settings->UseMultimon;

	return TRUE;
}


BOOL freerdp_client_write_rdp_file(rdpFile* file, char* name, BOOL unicode)
{
	BOOL success = FALSE;
	char* buffer;
	char *str;
	int len, len2;
	FILE* fp = NULL;
	WCHAR* unicodestr = NULL;

    len = _snprintf(str, len + 1, "%s %d", "test abcdefg", 123);
    printf("%s %d\n", str, len);

    free(str);

	len = freerdp_client_write_rdp_file_buffer(file, NULL, 0);
	if (len <= 0)
	{
		fprintf(stderr, "freerdp_client_write_rdp_file: Error determining buffer size.\n");
		return FALSE;
	}

	buffer = (char*) malloc((len + 1) * sizeof(char));
	len2 = freerdp_client_write_rdp_file_buffer(file, buffer, len + 1);
	if (len2 == len)
	{
		fp = fopen(name, "w+b");
		if (fp != NULL)
		{
			if (unicode)
			{
				ConvertToUnicode(CP_UTF8, 0, buffer, len, &unicodestr, 0);

				// Write multi-byte header
				fwrite(BOM_UTF16_LE, sizeof(BYTE), 2, fp);
				fwrite(unicodestr, 2, len, fp);

				free(unicodestr);
			}
			else
			{
				fwrite(buffer, 1, len, fp);
			}

			fflush(fp);
			fclose(fp);
		}
	}

	if (buffer != NULL)
		free(buffer);

	return success;
}

// TODO: Optimize by only writing the fields that have a value i.e ~((size_t) file->FieldName) != 0
size_t freerdp_client_write_rdp_file_buffer(rdpFile* file, char* buffer, size_t size)
{
	return _snprintf(buffer, size,
		"screen mode id:i:%d\n"
		"use multimon:i:%d\n"
		"desktopwidth:i:%d\n"
		"desktopheight:i:%d\n"
		"session bpp:i:%d\n"
		"winposstr:s:%s\n"
		"compression:i:%d\n"
		"keyboardhook:i:%d\n"
		"audiocapturemode:i:%d\n"
		"videoplaybackmode:i:%d\n"
		"connection type:i:%d\n"
		"networkautodetect:i:%d\n"
		"bandwidthautodetect:i:%d\n"
		"displayconnectionbar:i:%d\n"
		"enableworkspacereconnect:i:%d\n"
		"disable wallpaper:i:%d\n"
		"allow font smoothing:i:%d\n"
		"allow desktop composition:i:%d\n"
		"disable full window drag:i:%d\n"
		"disable menu anims:i:%d\n"
		"disable themes:i:%d\n"
		"disable cursor setting:i:%d\n"
		"bitmapcachepersistenable:i:%d\n"
		"full address:s:%s\n"
		"audiomode:i:%d\n"
		"redirectprinters:i:%d\n"
		"redirectcomports:i:%d\n"
		"redirectsmartcards:i:%d\n"
		"redirectclipboard:i:%d\n"
		"redirectposdevices:i:%d\n"
		"autoreconnection enabled:i:%d\n"
		"authentication level:i:%d\n"
		"prompt for credentials:i:%d\n"
		"negotiate security layer:i:%d\n"
		"remoteapplicationmode:i:%d\n"
		"alternate shell:s:%s\n"
		"shell working directory:s:%s\n"
		"gatewayhostname:s:%s\n"
		"gatewayusagemethod:i:%d\n"
		"gatewaycredentialssource:i:%d\n"
		"gatewayprofileusagemethod:i:%d\n"
		"promptcredentialonce:i:%d\n"
		"use redirection server name:i:%d\n"
		"rdgiskdcproxy:i:%d\n"
		"kdcproxyname:s:%s\n"
		"drivestoredirect:s:%s\n"
		"username:s:%s\n",
		file->ScreenModeId,
		file->UseMultiMon,
		file->DesktopWidth,
		file->DesktopHeight,
		file->SessionBpp,
		(~((size_t) file->WinPosStr) && file->WinPosStr != NULL) ? file->WinPosStr : "",
		file->Compression,
		file->KeyboardHook,
		file->AudioCaptureMode,
		file->VideoPlaybackMode,
		file->ConnectionType,
		file->NetworkAutoDetect,
		file->BandwidthAutoDetect,
		file->DisplayConnectionBar,
		file->EnableWorkspaceReconnect,
		file->DisableWallpaper,
		file->AllowFontSmoothing,
		file->AllowDesktopComposition,
		file->DisableFullWindowDrag,
		file->DisableMenuAnims,
		file->DisableThemes,
		file->DisableCursorSetting,
		file->BitmapCachePersistEnable,
		(~((size_t) file->FullAddress) && file->FullAddress != NULL) ? file->FullAddress : "",
		file->AudioMode,
		file->RedirectPrinters,
		file->RedirectComPorts,
		file->RedirectSmartCards,
		file->RedirectClipboard,
		file->RedirectPosDevices,
		file->AutoReconnectionEnabled,
		file->AuthenticationLevel,
		file->PromptForCredentials,
		file->NegotiateSecurityLayer,
		file->RemoteApplicationMode,
		(~((size_t) file->AlternateShell) && file->AlternateShell != NULL) ? file->AlternateShell : "",
		(~((size_t) file->ShellWorkingDirectory) && file->ShellWorkingDirectory != NULL) ? file->ShellWorkingDirectory : "",
		(~((size_t) file->GatewayHostname) && file->GatewayHostname != NULL) ? file->GatewayHostname : "",
		file->GatewayUsageMethod,
		file->GatewayCredentialsSource,
		file->GatewayProfileUsageMethod,
		file->PromptCredentialOnce,
		file->UseRedirectionServerName,
		file->RdgIsKdcProxy,
		(~((size_t) file->KdcProxyName) && file->KdcProxyName != NULL) ? file->KdcProxyName : "",
		(~((size_t) file->DrivesToRedirect) && file->DrivesToRedirect != NULL) ? file->DrivesToRedirect : "",
		(~((size_t) file->Username) && file->Username != NULL) ? file->Username : "");
}

BOOL freerdp_client_populate_settings_from_rdp_file(rdpFile* file, rdpSettings* settings)
{
	if (~((size_t) file->Domain))
		settings->Domain = file->Domain;

	if (~((size_t) file->Username))
	{
		char* user;
		char* domain;

		freerdp_parse_username(file->Username, &user, &domain);

		settings->Username = user;

		if (domain != NULL)
			settings->Domain = domain;
	}

	if (~file->ServerPort)
		settings->ServerPort = file->ServerPort;
	if (~((size_t) file->FullAddress))
		settings->ServerHostname = file->FullAddress;
	if (~file->DesktopWidth)
		settings->DesktopWidth = file->DesktopWidth;
	if (~file->DesktopHeight)
		settings->DesktopHeight = file->DesktopHeight;
	if (~file->SessionBpp)
		settings->ColorDepth = file->SessionBpp;
	if (~file->ConnectToConsole)
		settings->ConsoleSession = file->ConnectToConsole;
	if (~file->AdministrativeSession)
		settings->ConsoleSession = file->AdministrativeSession;
	if (~file->NegotiateSecurityLayer)
		settings->NegotiateSecurityLayer = file->NegotiateSecurityLayer;
	if (~file->EnableCredSSPSupport)
		settings->NlaSecurity = file->EnableCredSSPSupport;
	if (~((size_t) file->AlternateShell))
		settings->AlternateShell = file->AlternateShell;
	if (~((size_t) file->ShellWorkingDirectory))
		settings->ShellWorkingDirectory = file->ShellWorkingDirectory;
	
	if (~file->ConnectionType)
	{
		freerdp_set_connection_type(settings, file->ConnectionType);
	}

	if (~file->AudioMode)
	{
		if (file->AudioMode == AUDIO_MODE_REDIRECT)
		{
			settings->AudioPlayback = TRUE;
		}
		else if (file->AudioMode == AUDIO_MODE_PLAY_ON_SERVER)
		{
			settings->RemoteConsoleAudio = TRUE;
		}
		else if (file->AudioMode == AUDIO_MODE_NONE)
		{
			settings->AudioPlayback = FALSE;
			settings->RemoteConsoleAudio = FALSE;
		}
	}

	if (~((size_t) file->GatewayHostname))
		settings->GatewayHostname = file->GatewayHostname;
	if (~file->GatewayUsageMethod)
		settings->GatewayUsageMethod = TRUE;
	if (~file->PromptCredentialOnce)
		settings->GatewayUseSameCredentials = TRUE;
	
	if (~file->RemoteApplicationMode)
		settings->RemoteApplicationMode = file->RemoteApplicationMode;
	if (~((size_t) file->RemoteApplicationProgram))
		settings->RemoteApplicationProgram = file->RemoteApplicationProgram;
	if (~((size_t) file->RemoteApplicationName))
		settings->RemoteApplicationName = file->RemoteApplicationName;
	if (~((size_t) file->RemoteApplicationIcon))
		settings->RemoteApplicationIcon = file->RemoteApplicationIcon;
	if (~((size_t) file->RemoteApplicationFile))
		settings->RemoteApplicationFile = file->RemoteApplicationFile;
	if (~((size_t) file->RemoteApplicationGuid))
		settings->RemoteApplicationGuid = file->RemoteApplicationGuid;
	if (~((size_t) file->RemoteApplicationCmdLine))
		settings->RemoteApplicationCmdLine = file->RemoteApplicationCmdLine;

	if (~file->SpanMonitors)
		settings->SpanMonitors = file->SpanMonitors;
	if (~file->UseMultiMon)
		settings->UseMultimon = file->UseMultiMon;

	return TRUE;
}

rdpFile* freerdp_client_rdp_file_new()
{
	rdpFile* file;

	file = (rdpFile*) malloc(sizeof(rdpFile));
	FillMemory(file, sizeof(rdpFile), 0xFF);

	return file;
}

void freerdp_client_rdp_file_free(rdpFile* file)
{
	free(file);
}
