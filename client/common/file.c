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

//#define DEBUG_CLIENT_FILE	1

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

void freerdp_client_add_option(rdpFile* file, char* option)
{
	while ((file->argc + 1) > file->argSize)
	{
		file->argSize *= 2;
		file->argv = (char**) realloc(file->argv, file->argSize * sizeof(char*));
	}

	file->argv[file->argc] = _strdup(option);
	(file->argc)++;
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

void freerdp_client_parse_rdp_file_option_unicode(rdpFile* file, WCHAR* option)
{
	char* optionA = NULL;

	ConvertFromUnicode(CP_UTF8, 0, option, -1, &optionA, 0, NULL, NULL);
	freerdp_client_add_option(file, optionA);

	free(optionA);
}

void freerdp_client_parse_rdp_file_option_ascii(rdpFile* file, char* option)
{
	freerdp_client_add_option(file, option);
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

	while (line)
	{
		length = strlen(line);

		if (length > 1)
		{
			beg = line;
			end = &line[length - 1];

			if (beg[0] == '/')
			{
				freerdp_client_parse_rdp_file_option_ascii(file, line);
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

			if (beg[0] == '/')
			{
				/* FreeRDP option */
				freerdp_client_parse_rdp_file_option_unicode(file, line);
				goto next_line;
			}

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

BOOL freerdp_client_parse_rdp_file(rdpFile* file, const char* name)
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
	{
		fclose(fp);
		return FALSE;
	}

	buffer = (BYTE*) malloc(file_size + 2);
	read_size = fread(buffer, file_size, 1, fp);

	if (!read_size)
	{
		if (!ferror(fp))
			read_size = file_size;
	}
	fclose(fp);

	if (read_size < 1)
	{
		free(buffer);
		buffer = NULL;
		return FALSE;
	}

	buffer[file_size] = '\0';
	buffer[file_size + 1] = '\0';

	return freerdp_client_parse_rdp_file_buffer(file, buffer, file_size);
}

#define WRITE_ALL_SETTINGS FALSE
#define SETTING_MODIFIED(_settings, _field) (WRITE_ALL_SETTINGS || _settings->settings_modified[FreeRDP_##_field])
#define SETTING_MODIFIED_SET(_target, _settings, _field) if SETTING_MODIFIED(_settings, _field) _target = _settings->_field

BOOL freerdp_client_populate_rdp_file_from_settings(rdpFile* file, rdpSettings* settings)
{
	SETTING_MODIFIED_SET(file->Domain, settings, Domain);
	SETTING_MODIFIED_SET(file->Username, settings, Username);
	SETTING_MODIFIED_SET(file->ServerPort, settings, ServerPort);
	SETTING_MODIFIED_SET(file->FullAddress, settings, ServerHostname);
	SETTING_MODIFIED_SET(file->DesktopWidth, settings, DesktopWidth);
	SETTING_MODIFIED_SET(file->DesktopHeight, settings, DesktopHeight);
	SETTING_MODIFIED_SET(file->SessionBpp, settings, ColorDepth);
	SETTING_MODIFIED_SET(file->ConnectToConsole, settings, ConsoleSession);
	SETTING_MODIFIED_SET(file->AdministrativeSession, settings, ConsoleSession);
	SETTING_MODIFIED_SET(file->NegotiateSecurityLayer, settings, NegotiateSecurityLayer);
	SETTING_MODIFIED_SET(file->EnableCredSSPSupport, settings, NlaSecurity);
	SETTING_MODIFIED_SET(file->AlternateShell, settings, AlternateShell);
	SETTING_MODIFIED_SET(file->ShellWorkingDirectory, settings, ShellWorkingDirectory);
	SETTING_MODIFIED_SET(file->ConnectionType, settings, ConnectionType);

	if (SETTING_MODIFIED(settings, AudioPlayback) || SETTING_MODIFIED(settings, RemoteConsoleAudio))
	{
		if (settings->AudioPlayback)
			file->AudioMode = AUDIO_MODE_REDIRECT;
		else if (settings->RemoteConsoleAudio)
			file->AudioMode = AUDIO_MODE_PLAY_ON_SERVER;
		else
			file->AudioMode = AUDIO_MODE_NONE;
	}

	SETTING_MODIFIED_SET(file->GatewayHostname, settings, GatewayHostname);
	SETTING_MODIFIED_SET(file->GatewayUsageMethod, settings, GatewayUsageMethod);
	SETTING_MODIFIED_SET(file->PromptCredentialOnce, settings, GatewayUseSameCredentials);

	SETTING_MODIFIED_SET(file->RemoteApplicationMode, settings, RemoteApplicationMode);
	SETTING_MODIFIED_SET(file->RemoteApplicationProgram, settings, RemoteApplicationProgram);
	SETTING_MODIFIED_SET(file->RemoteApplicationName, settings, RemoteApplicationName);
	SETTING_MODIFIED_SET(file->RemoteApplicationIcon, settings, RemoteApplicationIcon);
	SETTING_MODIFIED_SET(file->RemoteApplicationFile, settings, RemoteApplicationFile);
	SETTING_MODIFIED_SET(file->RemoteApplicationGuid, settings, RemoteApplicationGuid);
	SETTING_MODIFIED_SET(file->RemoteApplicationCmdLine, settings, RemoteApplicationCmdLine);

	SETTING_MODIFIED_SET(file->SpanMonitors, settings, SpanMonitors);
	SETTING_MODIFIED_SET(file->UseMultiMon, settings, UseMultimon);

	return TRUE;
}

BOOL freerdp_client_write_rdp_file(rdpFile* file, const char* name, BOOL unicode)
{
	int rc = 0;
	char* buffer;
	int len, len2;
	FILE* fp = NULL;
	WCHAR* unicodestr = NULL;

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

			rc = fflush(fp);
			rc = fclose(fp);
		}
	}

	if (buffer != NULL)
		free(buffer);

	return (rc == 0);
}

#define WRITE_RDP_FILE_DECLARE(_file, _buffer, _size) \
	rdpFile* __rdpFile = file; \
	char* __buffer = _buffer; \
	size_t __size = _size; \
	size_t __required_size = 0; \
	int __current = 0; \
	int __count = 0;  

#define WRITE_RDP_FILE_VALUE_INTEGER(_format, _field) \
if (~__rdpFile->_field) \
	{ \
		__count = sprintf_s(__buffer == NULL ? NULL : __buffer + __current, __buffer == NULL ? 0 : __size - __required_size, _format, (int) __rdpFile->_field); \
		__required_size += __count; \
		__current += __count; \
	}

#define WRITE_RDP_FILE_VALUE_STRING(_format, _field) \
	if (~((size_t) __rdpFile->_field) && __rdpFile->_field != NULL) \
	{ \
		__count = sprintf_s(__buffer == NULL ? NULL : __buffer + __current, __buffer == NULL ? 0 : __size - __required_size, _format, __rdpFile->_field); \
		__required_size += __count; \
		__current += __count; \
	}

#define WRITE_RDP_FILE_VALUE_RETURN \
	return __required_size;

size_t freerdp_client_write_rdp_file_buffer(rdpFile* file, char* buffer, size_t size)
{
	WRITE_RDP_FILE_DECLARE(file, buffer, size)

	WRITE_RDP_FILE_VALUE_INTEGER("screen mode id:i:%d\n", ScreenModeId);
	WRITE_RDP_FILE_VALUE_INTEGER("use multimon:i:%d\n", UseMultiMon);
	WRITE_RDP_FILE_VALUE_INTEGER("desktopwidth:i:%d\n", DesktopWidth);
	WRITE_RDP_FILE_VALUE_INTEGER("desktopheight:i:%d\n", DesktopHeight);
	WRITE_RDP_FILE_VALUE_INTEGER("session bpp:i:%d\n", SessionBpp);
	WRITE_RDP_FILE_VALUE_STRING("winposstr:s:%s\n", WinPosStr);
	WRITE_RDP_FILE_VALUE_INTEGER("compression:i:%d\n", Compression);
	WRITE_RDP_FILE_VALUE_INTEGER("keyboardhook:i:%d\n", KeyboardHook);
	WRITE_RDP_FILE_VALUE_INTEGER("audiocapturemode:i:%d\n", AudioCaptureMode);
	WRITE_RDP_FILE_VALUE_INTEGER("videoplaybackmode:i:%d\n", VideoPlaybackMode);
	WRITE_RDP_FILE_VALUE_INTEGER("connection type:i:%d\n", ConnectionType);
	WRITE_RDP_FILE_VALUE_INTEGER("networkautodetect:i:%d\n", NetworkAutoDetect);
	WRITE_RDP_FILE_VALUE_INTEGER("bandwidthautodetect:i:%d\n", BandwidthAutoDetect);
	WRITE_RDP_FILE_VALUE_INTEGER("displayconnectionbar:i:%d\n", DisplayConnectionBar);
	WRITE_RDP_FILE_VALUE_INTEGER("enableworkspacereconnect:i:%d\n", EnableWorkspaceReconnect);
	WRITE_RDP_FILE_VALUE_INTEGER("disable wallpaper:i:%d\n", DisableWallpaper);
	WRITE_RDP_FILE_VALUE_INTEGER("allow font smoothing:i:%d\n", AllowFontSmoothing);
	WRITE_RDP_FILE_VALUE_INTEGER("allow desktop composition:i:%d\n", AllowDesktopComposition);
	WRITE_RDP_FILE_VALUE_INTEGER("disable full window drag:i:%d\n", DisableFullWindowDrag);
	WRITE_RDP_FILE_VALUE_INTEGER("disable menu anims:i:%d\n", DisableMenuAnims);
	WRITE_RDP_FILE_VALUE_INTEGER("disable themes:i:%d\n", DisableThemes);
	WRITE_RDP_FILE_VALUE_INTEGER("disable cursor setting:i:%d\n", DisableCursorSetting);
	WRITE_RDP_FILE_VALUE_INTEGER("bitmapcachepersistenable:i:%d\n", BitmapCachePersistEnable);
	WRITE_RDP_FILE_VALUE_STRING("full address:s:%s\n", FullAddress);
	WRITE_RDP_FILE_VALUE_INTEGER("audiomode:i:%d\n", AudioMode);
	WRITE_RDP_FILE_VALUE_INTEGER("redirectprinters:i:%d\n", RedirectPrinters);
	WRITE_RDP_FILE_VALUE_INTEGER("redirectcomports:i:%d\n", RedirectComPorts);
	WRITE_RDP_FILE_VALUE_INTEGER("redirectsmartcards:i:%d\n", RedirectSmartCards);
	WRITE_RDP_FILE_VALUE_INTEGER("redirectclipboard:i:%d\n", RedirectClipboard);
	WRITE_RDP_FILE_VALUE_INTEGER("redirectposdevices:i:%d\n", RedirectPosDevices);
	WRITE_RDP_FILE_VALUE_INTEGER("autoreconnection enabled:i:%d\n", AutoReconnectionEnabled);
	WRITE_RDP_FILE_VALUE_INTEGER("authentication level:i:%d\n", AuthenticationLevel);
	WRITE_RDP_FILE_VALUE_INTEGER("prompt for credentials:i:%d\n", PromptForCredentials);
	WRITE_RDP_FILE_VALUE_INTEGER("negotiate security layer:i:%d\n", NegotiateSecurityLayer);
	WRITE_RDP_FILE_VALUE_INTEGER("remoteapplicationmode:i:%d\n", RemoteApplicationMode);
	WRITE_RDP_FILE_VALUE_STRING("alternate shell:s:%s\n", AlternateShell);
	WRITE_RDP_FILE_VALUE_STRING("shell working directory:s:%s\n", ShellWorkingDirectory);
	WRITE_RDP_FILE_VALUE_STRING("gatewayhostname:s:%s\n", GatewayHostname);
	WRITE_RDP_FILE_VALUE_INTEGER("gatewayusagemethod:i:%d\n", GatewayUsageMethod);
	WRITE_RDP_FILE_VALUE_INTEGER("gatewaycredentialssource:i:%d\n", GatewayCredentialsSource);
	WRITE_RDP_FILE_VALUE_INTEGER("gatewayprofileusagemethod:i:%d\n", GatewayProfileUsageMethod);
	WRITE_RDP_FILE_VALUE_INTEGER("promptcredentialonce:i:%d\n", PromptCredentialOnce);
	WRITE_RDP_FILE_VALUE_INTEGER("use redirection server name:i:%d\n", UseRedirectionServerName);
	WRITE_RDP_FILE_VALUE_INTEGER("rdgiskdcproxy:i:%d\n", RdgIsKdcProxy);
	WRITE_RDP_FILE_VALUE_STRING("kdcproxyname:s:%s\n", KdcProxyName);
	WRITE_RDP_FILE_VALUE_STRING("drivestoredirect:s:%s\n", DrivesToRedirect);
	WRITE_RDP_FILE_VALUE_STRING("username:s:%s\n", Username);
	WRITE_RDP_FILE_VALUE_STRING("domain:s:%s\n", Domain);

	WRITE_RDP_FILE_VALUE_RETURN
}

BOOL freerdp_client_populate_settings_from_rdp_file(rdpFile* file, rdpSettings* settings)
{
	if (~((size_t) file->Domain))
		freerdp_set_param_string(settings, FreeRDP_Domain, file->Domain);

	if (~((size_t) file->Username))
	{
		char* user = NULL;
		char* domain = NULL;

		freerdp_parse_username(file->Username, &user, &domain);
		freerdp_set_param_string(settings, FreeRDP_Username, user);

		if (domain)
			freerdp_set_param_string(settings, FreeRDP_Domain, domain);

		if (user)
			free(user);

		if (domain)
			free(domain);
	}

	if (~file->ServerPort)
		freerdp_set_param_uint32(settings, FreeRDP_ServerPort, file->ServerPort);
	if (~((size_t) file->FullAddress))
		freerdp_set_param_string(settings, FreeRDP_ServerHostname, file->FullAddress);

	if (~file->DesktopWidth)
		freerdp_set_param_uint32(settings, FreeRDP_DesktopWidth, file->DesktopWidth);
	if (~file->DesktopHeight)
		freerdp_set_param_uint32(settings, FreeRDP_DesktopHeight, file->DesktopHeight);
	if (~file->SessionBpp)
		freerdp_set_param_uint32(settings, FreeRDP_ColorDepth, file->SessionBpp);
	if (~file->ConnectToConsole)
		freerdp_set_param_uint32(settings, FreeRDP_ConsoleSession, file->ConnectToConsole);
	if (~file->AdministrativeSession)
		freerdp_set_param_uint32(settings, FreeRDP_ConsoleSession, file->AdministrativeSession);
	if (~file->NegotiateSecurityLayer)
		freerdp_set_param_uint32(settings, FreeRDP_NegotiateSecurityLayer, file->NegotiateSecurityLayer);
	if (~file->EnableCredSSPSupport)
		freerdp_set_param_uint32(settings, FreeRDP_NlaSecurity, file->EnableCredSSPSupport);
	if (~((size_t) file->AlternateShell))
		freerdp_set_param_string(settings, FreeRDP_AlternateShell, file->AlternateShell);
	if (~((size_t) file->ShellWorkingDirectory))
		freerdp_set_param_string(settings, FreeRDP_ShellWorkingDirectory, file->ShellWorkingDirectory);
	
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
		 * 0: The remote session will appear in a window.
		 * 1: The remote session will appear full screen.
		 */

		freerdp_set_param_bool(settings, FreeRDP_Fullscreen,
				(file->ScreenModeId == 1) ? TRUE : FALSE);
	}

	if (~((size_t) file->LoadBalanceInfo))
	{
		settings->LoadBalanceInfo = (BYTE*) _strdup(file->LoadBalanceInfo);
		settings->LoadBalanceInfoLength = strlen((char*) settings->LoadBalanceInfo);
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
		 * 0: If server authentication fails, connect to the computer without warning (Connect and don’t warn me).
		 * 1: If server authentication fails, do not establish a connection (Do not connect).
		 * 2: If server authentication fails, show a warning and allow me to connect or refuse the connection (Warn me).
		 * 3: No authentication requirement is specified.
		 */

		freerdp_set_param_bool(settings, FreeRDP_IgnoreCertificate,
				(file->AuthenticationLevel == 0) ? TRUE : FALSE);
	}

	if (~file->ConnectionType)
		freerdp_set_param_uint32(settings, FreeRDP_ConnectionType, file->ConnectionType);

	if (~file->AudioMode)
	{
		if (file->AudioMode == AUDIO_MODE_REDIRECT)
		{
			freerdp_set_param_bool(settings, FreeRDP_AudioPlayback, TRUE);
		}
		else if (file->AudioMode == AUDIO_MODE_PLAY_ON_SERVER)
		{
			freerdp_set_param_bool(settings, FreeRDP_RemoteConsoleAudio, TRUE);
		}
		else if (file->AudioMode == AUDIO_MODE_NONE)
		{
			freerdp_set_param_bool(settings, FreeRDP_AudioPlayback, FALSE);
			freerdp_set_param_bool(settings, FreeRDP_RemoteConsoleAudio, FALSE);
		}
	}

	if (~file->Compression)
		freerdp_set_param_bool(settings, FreeRDP_CompressionEnabled, file->Compression);

	if (~((size_t) file->GatewayHostname))
		freerdp_set_param_string(settings, FreeRDP_GatewayHostname, file->GatewayHostname);

	if (~file->GatewayUsageMethod)
	{
		freerdp_set_param_uint32(settings, FreeRDP_GatewayUsageMethod, file->GatewayUsageMethod);

		if (file->GatewayUsageMethod == TSC_PROXY_MODE_DIRECT)
			freerdp_set_param_bool(settings, FreeRDP_GatewayEnabled, TRUE);
		else if (file->GatewayUsageMethod == TSC_PROXY_MODE_NONE_DETECT)
			freerdp_set_param_bool(settings, FreeRDP_GatewayEnabled, FALSE);
	}

	if (~file->PromptCredentialOnce)
		freerdp_set_param_bool(settings, FreeRDP_GatewayUseSameCredentials, TRUE);
	
	if (~file->RemoteApplicationMode)
		freerdp_set_param_bool(settings, FreeRDP_RemoteApplicationMode, file->RemoteApplicationMode);
	if (~((size_t) file->RemoteApplicationProgram))
		freerdp_set_param_string(settings, FreeRDP_RemoteApplicationProgram, file->RemoteApplicationProgram);
	if (~((size_t) file->RemoteApplicationName))
		freerdp_set_param_string(settings, FreeRDP_RemoteApplicationName, file->RemoteApplicationName);
	if (~((size_t) file->RemoteApplicationIcon))
		freerdp_set_param_string(settings, FreeRDP_RemoteApplicationIcon, file->RemoteApplicationIcon);
	if (~((size_t) file->RemoteApplicationFile))
		freerdp_set_param_string(settings, FreeRDP_RemoteApplicationGuid, file->RemoteApplicationGuid);
	if (~((size_t) file->RemoteApplicationGuid))
		freerdp_set_param_string(settings, FreeRDP_RemoteApplicationGuid, file->RemoteApplicationGuid);
	if (~((size_t) file->RemoteApplicationCmdLine))
		freerdp_set_param_string(settings, FreeRDP_RemoteApplicationCmdLine, file->RemoteApplicationCmdLine);

	if (~file->SpanMonitors)
		freerdp_set_param_bool(settings, FreeRDP_SpanMonitors, file->SpanMonitors);
	if (~file->UseMultiMon)
		freerdp_set_param_bool(settings, FreeRDP_UseMultimon, file->UseMultiMon);

	if (~file->AllowFontSmoothing)
		freerdp_set_param_bool(settings, FreeRDP_AllowFontSmoothing, file->AllowFontSmoothing);
	if (~file->DisableWallpaper)
		freerdp_set_param_bool(settings, FreeRDP_DisableWallpaper, file->DisableWallpaper);
	if (~file->DisableFullWindowDrag)
		freerdp_set_param_bool(settings, FreeRDP_DisableFullWindowDrag, file->DisableFullWindowDrag);
	if (~file->DisableMenuAnims)
		freerdp_set_param_bool(settings, FreeRDP_DisableMenuAnims, file->DisableMenuAnims);
	if (~file->DisableThemes)
		freerdp_set_param_bool(settings, FreeRDP_DisableThemes, file->DisableThemes);
	if (~file->AllowDesktopComposition)
		freerdp_set_param_bool(settings, FreeRDP_DisableCursorShadow, file->AllowDesktopComposition);

	if (~file->BitmapCachePersistEnable)
		freerdp_set_param_bool(settings, FreeRDP_BitmapCachePersistEnabled, file->BitmapCachePersistEnable);

	if (~file->DisableRemoteAppCapsCheck)
		freerdp_set_param_bool(settings, FreeRDP_DisableRemoteAppCapsCheck, file->DisableRemoteAppCapsCheck);

	if (~file->AutoReconnectionEnabled)
		freerdp_set_param_bool(settings, FreeRDP_AutoReconnectionEnabled, file->AutoReconnectionEnabled);
	if (~file->AutoReconnectMaxRetries)
		freerdp_set_param_uint32(settings, FreeRDP_AutoReconnectMaxRetries, file->AutoReconnectMaxRetries);

	if (~file->RedirectSmartCards)
		freerdp_set_param_bool(settings, FreeRDP_RedirectSmartCards, file->RedirectSmartCards);

	if (~file->RedirectClipboard)
		freerdp_set_param_bool(settings, FreeRDP_RedirectClipboard, file->RedirectClipboard);

	if (~file->RedirectPrinters)
		freerdp_set_param_bool(settings, FreeRDP_RedirectPrinters, file->RedirectPrinters);

	if (~file->RedirectDrives)
		freerdp_set_param_bool(settings, FreeRDP_RedirectDrives, file->RedirectDrives);

	if (~file->RedirectPosDevices)
	{
		freerdp_set_param_bool(settings, FreeRDP_RedirectSerialPorts, file->RedirectComPorts);
		freerdp_set_param_bool(settings, FreeRDP_RedirectParallelPorts, file->RedirectComPorts);
	}

	if (~file->RedirectComPorts)
	{
		freerdp_set_param_bool(settings, FreeRDP_RedirectSerialPorts, file->RedirectComPorts);
		freerdp_set_param_bool(settings, FreeRDP_RedirectParallelPorts, file->RedirectComPorts);
	}

	if (~file->RedirectDirectX)
	{
		/* What is this?! */
	}

	if (~((size_t) file->DevicesToRedirect))
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

		freerdp_set_param_bool(settings, FreeRDP_RedirectDrives, TRUE);
	}

	if (~((size_t) file->DrivesToRedirect))
	{
		/*
		 * Drives to redirect:
		 *
		 * Very similar to DevicesToRedirect, but can contain a
		 * comma-separated list of drive letters to redirect.
		 */

		freerdp_set_param_bool(settings, FreeRDP_RedirectDrives, TRUE);
	}

	if (file->argc > 1)
	{
		freerdp_client_parse_command_line_arguments(file->argc, file->argv, settings);
	}

	return TRUE;
}

rdpFile* freerdp_client_rdp_file_new()
{
	rdpFile* file;

	file = (rdpFile*) malloc(sizeof(rdpFile));

	if (file)
	{
		FillMemory(file, sizeof(rdpFile), 0xFF);

		file->argc = 0;
		file->argSize = 32;
		file->argv = (char**) malloc(file->argSize * sizeof(char*));

		freerdp_client_add_option(file, "freerdp");
	}

	return file;
}

void freerdp_client_rdp_file_free(rdpFile* file)
{
	int i;

	if (file)
	{
		if (file->argv)
		{
			for (i = 0; i < file->argc; i++)
				free(file->argv[i]);

			free(file->argv);
		}

		free(file);
	}
}
