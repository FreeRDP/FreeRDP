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

#include <errno.h>

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

#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <freerdp/log.h>
#define TAG CLIENT_TAG("common")

//#define DEBUG_CLIENT_FILE	1

static BYTE BOM_UTF16_LE[2] = { 0xFF, 0xFE };

#define INVALID_INTEGER_VALUE		0xFFFFFFFF

/*
 * Set an integer in a rdpFile
 *
 * @return 0 if a standard name was set, 1 for a non-standard name, -1 on error
 *
 */

static int freerdp_client_rdp_file_set_integer(rdpFile* file, const char* name, int value,
        int index)
{
	int standard = 1;
#ifdef DEBUG_CLIENT_FILE
	WLog_DBG(TAG,  "%s:i:%d", name, value);
#endif

	if (_stricmp(name, "use multimon") == 0)
		file->UseMultiMon = value;
	else if (_stricmp(name, "screen mode id") == 0)
		file->ScreenModeId = value;
	else if (_stricmp(name, "span monitors") == 0)
		file->SpanMonitors = value;
	else if (_stricmp(name, "smart sizing") == 0)
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
		standard = 1;

	if (index >= 0)
	{
		file->lines[index].name = _strdup(name);

		if (!file->lines[index].name)
			return -1;

		file->lines[index].iValue = (DWORD) value;
		file->lines[index].flags = RDP_FILE_LINE_FLAG_FORMATTED;
		file->lines[index].flags |= RDP_FILE_LINE_FLAG_TYPE_INTEGER;

		if (standard)
			file->lines[index].flags |= RDP_FILE_LINE_FLAG_STANDARD;

		file->lines[index].valueLength = 0;
	}

	return standard;
}

static BOOL freerdp_client_parse_rdp_file_integer(rdpFile* file, const char* name,
        const char* value, int index)
{
	long ivalue;
	errno = 0;
	ivalue = strtol(value, NULL, 0);

	if ((errno != 0) || (ivalue < INT32_MIN) || (ivalue > INT32_MAX))
	{
		WLog_ERR(TAG, "Failed to convert RDP file integer option %s [value=%s]", name, value);
		return FALSE;
	}

	if (freerdp_client_rdp_file_set_integer(file, name, ivalue, index) < 0)
		return FALSE;

	return TRUE;
}

/**
 *
 * @param file rdpFile
 * @param name name of the string
 * @param value value of the string to set
 * @param index line index of the rdpFile
 * @return 0 on success, 1 if the key wasn't found (not a standard key), -1 on error
 */

static int freerdp_client_rdp_file_set_string(rdpFile* file, const char* name, const char* value,
        int index)
{
	int standard = 0;
	LPSTR* tmp = NULL;
#ifdef DEBUG_CLIENT_FILE
	WLog_DBG(TAG,  "%s:s:%s", name, value);
#endif

	if (!file)
		return -1;

	if (_stricmp(name, "username") == 0)
		tmp = &file->Username;
	else if (_stricmp(name, "domain") == 0)
		tmp = &file->Domain;
	else if (_stricmp(name, "password") == 0)
		tmp = &file->Password;
	else if (_stricmp(name, "full address") == 0)
		tmp = &file->FullAddress;
	else if (_stricmp(name, "alternate full address") == 0)
		tmp = &file->AlternateFullAddress;
	else if (_stricmp(name, "usbdevicestoredirect") == 0)
		tmp = &file->UsbDevicesToRedirect;
	else if (_stricmp(name, "loadbalanceinfo") == 0)
		tmp = &file->LoadBalanceInfo;
	else if (_stricmp(name, "remoteapplicationname") == 0)
		tmp = &file->RemoteApplicationName;
	else if (_stricmp(name, "remoteapplicationicon") == 0)
		tmp = &file->RemoteApplicationIcon;
	else if (_stricmp(name, "remoteapplicationprogram") == 0)
		tmp = &file->RemoteApplicationProgram;
	else if (_stricmp(name, "remoteapplicationfile") == 0)
		tmp = &file->RemoteApplicationFile;
	else if (_stricmp(name, "remoteapplicationguid") == 0)
		tmp = &file->RemoteApplicationGuid;
	else if (_stricmp(name, "remoteapplicationcmdline") == 0)
		tmp = &file->RemoteApplicationCmdLine;
	else if (_stricmp(name, "alternate shell") == 0)
		tmp = &file->AlternateShell;
	else if (_stricmp(name, "shell working directory") == 0)
		tmp = &file->ShellWorkingDirectory;
	else if (_stricmp(name, "gatewayhostname") == 0)
		tmp = &file->GatewayHostname;
	else if (_stricmp(name, "gatewayaccesstoken") == 0)
		tmp = &file->GatewayAccessToken;
	else if (_stricmp(name, "kdcproxyname") == 0)
		tmp = &file->KdcProxyName;
	else if (_stricmp(name, "drivestoredirect") == 0)
		tmp = &file->DrivesToRedirect;
	else if (_stricmp(name, "devicestoredirect") == 0)
		tmp = &file->DevicesToRedirect;
	else if (_stricmp(name, "winposstr") == 0)
		tmp = &file->WinPosStr;
	else if (_stricmp(name, "pcb") == 0)
		tmp = &file->PreconnectionBlob;
	else
		standard = 1;

	if (tmp && !(*tmp = _strdup(value)))
		return -1;

	if (index >= 0)
	{
		if (!file->lines)
			return -1;

		file->lines[index].name = _strdup(name);
		file->lines[index].sValue = _strdup(value);

		if (!file->lines[index].name || !file->lines[index].sValue)
		{
			free(file->lines[index].name);
			free(file->lines[index].sValue);
			return -1;
		}

		file->lines[index].flags = RDP_FILE_LINE_FLAG_FORMATTED;
		file->lines[index].flags |= RDP_FILE_LINE_FLAG_TYPE_STRING;

		if (standard == 0)
			file->lines[index].flags |= RDP_FILE_LINE_FLAG_STANDARD;

		file->lines[index].valueLength = 0;
	}

	return standard;
}

static BOOL freerdp_client_add_option(rdpFile* file, char* option)
{
	while ((file->argc + 1) > file->argSize)
	{
		int new_size;
		char** new_argv;
		new_size = file->argSize * 2;
		new_argv = (char**) realloc(file->argv, new_size * sizeof(char*));

		if (!new_argv)
			return FALSE;

		file->argv = new_argv;
		file->argSize = new_size;
	}

	file->argv[file->argc] = _strdup(option);

	if (!file->argv[file->argc])
		return FALSE;

	(file->argc)++;
	return TRUE;
}

static int freerdp_client_parse_rdp_file_add_line(rdpFile* file, char* line, int index)
{
	if (index < 0)
		index = file->lineCount;

	while ((file->lineCount + 1) > file->lineSize)
	{
		int new_size;
		rdpFileLine* new_line;
		new_size = file->lineSize * 2;
		new_line = (rdpFileLine*) realloc(file->lines, new_size * sizeof(rdpFileLine));

		if (!new_line)
			return -1;

		file->lines = new_line;
		file->lineSize = new_size;
	}

	ZeroMemory(&(file->lines[file->lineCount]), sizeof(rdpFileLine));
	file->lines[file->lineCount].text = _strdup(line);

	if (!file->lines[file->lineCount].text)
		return -1;

	file->lines[file->lineCount].index = index;
	(file->lineCount)++;
	return index;
}

static BOOL freerdp_client_parse_rdp_file_string(rdpFile* file, char* name, char* value,
        int index)
{
	BOOL ret = TRUE;
	char* valueA = _strdup(value);

	if (!valueA)
	{
		WLog_ERR(TAG, "Failed to convert RDP file string option %s [value=%s]", name, value);
		return FALSE;
	}

	if (freerdp_client_rdp_file_set_string(file, name, valueA, index) == -1)
		ret = FALSE;

	free(valueA);
	return ret;
}

static BOOL freerdp_client_parse_rdp_file_option(rdpFile* file, char* option, int index)
{
	return freerdp_client_add_option(file, option);
}

BOOL freerdp_client_parse_rdp_file_buffer(rdpFile* file, const BYTE* buffer,
        size_t size)
{
	BOOL rc = FALSE;
	int index;
	size_t length;
	char* line;
	char* type;
	char* context;
	char* d1, *d2;
	char* beg;
	char* name, *value;
	char* copy = NULL;

	if (size < 2)
		return FALSE;

	if ((buffer[0] == BOM_UTF16_LE[0]) && (buffer[1] == BOM_UTF16_LE[1]))
	{
		size = size / 2 - 1;

		if (ConvertFromUnicode(CP_UTF8, 0, (LPCWSTR)(&buffer[2]), size, &copy, 0, NULL, NULL) < 0)
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

	index = 0;
	line = strtok_s(copy, "\r\n", &context);

	while (line)
	{
		length = strnlen(line, size);

		if (length > 1)
		{
			beg = line;

			if (freerdp_client_parse_rdp_file_add_line(file, line, index) == -1)
				goto fail;

			if (beg[0] == '/')
			{
				if (!freerdp_client_parse_rdp_file_option(file, line, index))
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

			if (*type == 'i')
			{
				/* integer type */
				if (!freerdp_client_parse_rdp_file_integer(file, name, value, index))
					goto fail;
			}
			else if (*type == 's')
			{
				/* string type */
				if (!freerdp_client_parse_rdp_file_string(file, name, value, index))
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
		index++;
	}

	rc = TRUE;
fail:
	free(copy);
	return rc;
}

BOOL freerdp_client_parse_rdp_file(rdpFile* file, const char* name)
{
	BOOL status;
	BYTE* buffer;
	FILE* fp = NULL;
	size_t read_size;
	INT64 file_size;
	fp = fopen(name, "r");

	if (!fp)
	{
		WLog_ERR(TAG, "Failed to open RDP file %s", name);
		return FALSE;
	}

	_fseeki64(fp, 0, SEEK_END);
	file_size = _ftelli64(fp);
	_fseeki64(fp, 0, SEEK_SET);

	if (file_size < 1)
	{
		WLog_ERR(TAG, "RDP file %s is empty", name);
		fclose(fp);
		return FALSE;
	}

	buffer = (BYTE*) malloc(file_size + 2);

	if (!buffer)
	{
		fclose(fp);
		return FALSE;
	}

	read_size = fread(buffer, file_size, 1, fp);

	if (!read_size)
	{
		if (!ferror(fp))
			read_size = file_size;
	}

	fclose(fp);

	if (read_size < 1)
	{
		WLog_ERR(TAG, "Could not read from RDP file %s", name);
		free(buffer);
		return FALSE;
	}

	buffer[file_size] = '\0';
	buffer[file_size + 1] = '\0';
	status = freerdp_client_parse_rdp_file_buffer(file, buffer, file_size);
	free(buffer);
	return status;
}

#define WRITE_ALL_SETTINGS TRUE
#define SETTING_MODIFIED(_settings, _field) (WRITE_ALL_SETTINGS || _settings->SettingsModified[FreeRDP_##_field])
#define SETTING_MODIFIED_SET(_target, _settings, _field) if SETTING_MODIFIED(_settings, _field) _target = _settings->_field
#define SETTING_MODIFIED_SET_STRING(_target, _settings, _field) do { if SETTING_MODIFIED(_settings, _field) _target = _strdup(_settings->_field); \
		if (!_target) return FALSE; \
	} while (0)

BOOL freerdp_client_populate_rdp_file_from_settings(rdpFile* file, const rdpSettings* settings)
{
	SETTING_MODIFIED_SET_STRING(file->Domain, settings, Domain);
	SETTING_MODIFIED_SET_STRING(file->Username, settings, Username);
	SETTING_MODIFIED_SET_STRING(file->Password, settings, Password);
	SETTING_MODIFIED_SET(file->ServerPort, settings, ServerPort);
	SETTING_MODIFIED_SET_STRING(file->FullAddress, settings, ServerHostname);
	SETTING_MODIFIED_SET(file->DesktopWidth, settings, DesktopWidth);
	SETTING_MODIFIED_SET(file->DesktopHeight, settings, DesktopHeight);
	SETTING_MODIFIED_SET(file->SessionBpp, settings, ColorDepth);
	SETTING_MODIFIED_SET(file->ConnectToConsole, settings, ConsoleSession);
	SETTING_MODIFIED_SET(file->AdministrativeSession, settings, ConsoleSession);
	SETTING_MODIFIED_SET(file->NegotiateSecurityLayer, settings, NegotiateSecurityLayer);
	SETTING_MODIFIED_SET(file->EnableCredSSPSupport, settings, NlaSecurity);
	SETTING_MODIFIED_SET_STRING(file->AlternateShell, settings, AlternateShell);
	SETTING_MODIFIED_SET_STRING(file->ShellWorkingDirectory, settings, ShellWorkingDirectory);
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

	SETTING_MODIFIED_SET_STRING(file->GatewayHostname, settings, GatewayHostname);
	SETTING_MODIFIED_SET_STRING(file->GatewayAccessToken, settings, GatewayAccessToken);
	SETTING_MODIFIED_SET(file->GatewayUsageMethod, settings, GatewayUsageMethod);
	SETTING_MODIFIED_SET(file->PromptCredentialOnce, settings, GatewayUseSameCredentials);
	SETTING_MODIFIED_SET(file->RemoteApplicationMode, settings, RemoteApplicationMode);
	SETTING_MODIFIED_SET_STRING(file->RemoteApplicationProgram, settings, RemoteApplicationProgram);
	SETTING_MODIFIED_SET_STRING(file->RemoteApplicationName, settings, RemoteApplicationName);
	SETTING_MODIFIED_SET_STRING(file->RemoteApplicationIcon, settings, RemoteApplicationIcon);
	SETTING_MODIFIED_SET_STRING(file->RemoteApplicationFile, settings, RemoteApplicationFile);
	SETTING_MODIFIED_SET_STRING(file->RemoteApplicationGuid, settings, RemoteApplicationGuid);
	SETTING_MODIFIED_SET_STRING(file->RemoteApplicationCmdLine, settings, RemoteApplicationCmdLine);
	SETTING_MODIFIED_SET(file->SpanMonitors, settings, SpanMonitors);
	SETTING_MODIFIED_SET(file->UseMultiMon, settings, UseMultimon);
	SETTING_MODIFIED_SET_STRING(file->PreconnectionBlob, settings, PreconnectionBlob);
	return TRUE;
}

BOOL freerdp_client_write_rdp_file(const rdpFile* file, const char* name, BOOL unicode)
{
	FILE* fp;
	size_t size;
	char* buffer;
	int status = 0;
	WCHAR* unicodestr = NULL;
	size = freerdp_client_write_rdp_file_buffer(file, NULL, 0);
	buffer = (char*) calloc((size + 1), sizeof(char));

	if (freerdp_client_write_rdp_file_buffer(file, buffer, size + 1) != size)
	{
		WLog_ERR(TAG,  "freerdp_client_write_rdp_file: error writing to output buffer");
		free(buffer);
		return FALSE;
	}

	fp = fopen(name, "w+b");

	if (fp)
	{
		if (unicode)
		{
			int length;

			if (size > INT_MAX)
			{
				free(buffer);
				free(unicodestr);
				fclose(fp);
				return FALSE;
			}

			length = (int)size;
			ConvertToUnicode(CP_UTF8, 0, buffer, length, &unicodestr, 0);

			/* Write multi-byte header */
			if ((length < 0) ||
			    (fwrite(BOM_UTF16_LE, sizeof(BYTE), 2, fp) != 2) ||
			    (fwrite(unicodestr, 2, (size_t)length, fp) != (size_t)length))
			{
				free(buffer);
				free(unicodestr);
				fclose(fp);
				return FALSE;
			}

			free(unicodestr);
		}
		else
		{
			if (fwrite(buffer, 1, size, fp) != size)
			{
				free(buffer);
				fclose(fp);
				return FALSE;
			}
		}

		fflush(fp);
		status = fclose(fp);
	}

	free(buffer);
	return (status == 0) ? TRUE : FALSE;
}

size_t freerdp_client_write_rdp_file_buffer(const rdpFile* file, char* buffer, size_t size)
{
	int index;
	int length;
	char* output;
	rdpFileLine* line;

	if (!buffer)
		size = 0;

	output = buffer;

	for (index = 0; index < file->lineCount; index++)
	{
		line = &(file->lines[index]);
		length = (int) strlen(line->text);

		if (!buffer)
		{
			size += length + 1;
		}
		else
		{
			CopyMemory(output, line->text, length);
			output += length;
			*output = '\n';
			output++;
		}
	}

	if (buffer)
		size = (output - buffer);

	return size;
}

BOOL freerdp_client_populate_settings_from_rdp_file(rdpFile* file, rdpSettings* settings)
{
	if (~((size_t) file->Domain))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_Domain, file->Domain))
			return FALSE;
	}

	if (~((size_t) file->Username))
	{
		char* user = NULL;
		char* domain = NULL;

		if (!freerdp_parse_username(file->Username, &user, &domain))
			return FALSE;

		if (!freerdp_settings_set_string(settings, FreeRDP_Username, user))
			return FALSE;

		if (domain)
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

	if (~((size_t) file->FullAddress))
	{
		int port = -1;
		char* host = NULL;

		if (!freerdp_parse_hostname(file->FullAddress, &host, &port))
			return FALSE;

		if (!freerdp_settings_set_string(settings, FreeRDP_ServerHostname, host))
			return FALSE;

		free(host);

		if (port > 0)
		{
			if (!freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, (UINT32) port))
				return FALSE;
		}
	}

	if (~file->ServerPort)
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, file->ServerPort))
			return FALSE;
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
		if (!freerdp_settings_set_bool(settings, FreeRDP_ConsoleSession, file->ConnectToConsole))
			return FALSE;
	}

	if (~file->AdministrativeSession)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_ConsoleSession, file->AdministrativeSession))
			return FALSE;
	}

	if (~file->NegotiateSecurityLayer)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_NegotiateSecurityLayer,
		                               file->NegotiateSecurityLayer))
			return FALSE;
	}

	if (~file->EnableCredSSPSupport)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, file->EnableCredSSPSupport))
			return FALSE;
	}

	if (~((size_t) file->AlternateShell))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_AlternateShell, file->AlternateShell))
			return FALSE;
	}

	if (~((size_t) file->ShellWorkingDirectory))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_ShellWorkingDirectory,
		                                 file->ShellWorkingDirectory))
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
	}

	if (~((size_t) file->LoadBalanceInfo))
	{
		settings->LoadBalanceInfo = (BYTE*) _strdup(file->LoadBalanceInfo);

		if (!settings->LoadBalanceInfo)
			return FALSE;

		settings->LoadBalanceInfoLength = (int) strlen((char*) settings->LoadBalanceInfo);
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
		settings->AuthenticationLevel = file->AuthenticationLevel;
	}

	if (~file->ConnectionType)
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_ConnectionType, file->ConnectionType))
			return FALSE;
	}

	if (~file->AudioMode)
	{
		if (file->AudioMode == AUDIO_MODE_REDIRECT)
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_AudioPlayback, TRUE))
				return FALSE;
		}
		else if (file->AudioMode == AUDIO_MODE_PLAY_ON_SERVER)
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteConsoleAudio, TRUE))
				return FALSE;
		}
		else if (file->AudioMode == AUDIO_MODE_NONE)
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_AudioPlayback, FALSE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_RemoteConsoleAudio, FALSE))
				return FALSE;
		}
	}

	if (~file->Compression)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_CompressionEnabled, file->Compression))
			return FALSE;
	}

	if (~((size_t) file->GatewayHostname))
	{
		int port = -1;
		char* host = NULL;

		if (!freerdp_parse_hostname(file->GatewayHostname, &host, &port))
			return FALSE;

		if (!freerdp_settings_set_string(settings, FreeRDP_GatewayHostname, host))
			return FALSE;

		free(host);

		if (port > 0)
		{
			if (!freerdp_settings_set_uint32(settings, FreeRDP_GatewayPort, (UINT32) port))
				return FALSE;
		}
	}

	if (~((size_t) file->GatewayAccessToken))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_GatewayAccessToken, file->GatewayAccessToken))
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
		                               file->PromptCredentialOnce))
			return FALSE;
	}

	if (~file->RemoteApplicationMode)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteApplicationMode,
		                               file->RemoteApplicationMode))
			return FALSE;
	}

	if (~((size_t) file->RemoteApplicationProgram))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationProgram,
		                                 file->RemoteApplicationProgram))
			return FALSE;
	}

	if (~((size_t) file->RemoteApplicationName))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationName,
		                                 file->RemoteApplicationName))
			return FALSE;
	}

	if (~((size_t) file->RemoteApplicationIcon))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationIcon,
		                                 file->RemoteApplicationIcon))
			return FALSE;
	}

	if (~((size_t) file->RemoteApplicationFile))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationGuid,
										file->RemoteApplicationGuid))
			return FALSE;
	}

	if (~((size_t) file->RemoteApplicationCmdLine))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationCmdLine,
		                                 file->RemoteApplicationCmdLine))
			return FALSE;
	}

	if (~file->SpanMonitors)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_SpanMonitors, file->SpanMonitors))
			return FALSE;
	}

	if (~file->UseMultiMon)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_UseMultimon, file->UseMultiMon))
			return FALSE;
	}

	if (~file->AllowFontSmoothing)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_AllowFontSmoothing, file->AllowFontSmoothing))
			return FALSE;
	}

	if (~file->DisableWallpaper)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_DisableWallpaper, file->DisableWallpaper))
			return FALSE;
	}

	if (~file->DisableFullWindowDrag)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_DisableFullWindowDrag,
		                               file->DisableFullWindowDrag))
			return FALSE;
	}

	if (~file->DisableMenuAnims)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_DisableMenuAnims, file->DisableMenuAnims))
			return FALSE;
	}

	if (~file->DisableThemes)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_DisableThemes, file->DisableThemes))
			return FALSE;
	}

	if (~file->AllowDesktopComposition)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_AllowDesktopComposition,
		                               file->AllowDesktopComposition))
			return FALSE;
	}

	if (~file->BitmapCachePersistEnable)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_BitmapCachePersistEnabled,
		                               file->BitmapCachePersistEnable))
			return FALSE;
	}

	if (~file->DisableRemoteAppCapsCheck)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_DisableRemoteAppCapsCheck,
		                               file->DisableRemoteAppCapsCheck))
			return FALSE;
	}

	if (~file->AutoReconnectionEnabled)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_AutoReconnectionEnabled,
		                               file->AutoReconnectionEnabled))
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
		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectSmartCards, file->RedirectSmartCards))
			return FALSE;
	}

	if (~file->RedirectClipboard)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectClipboard, file->RedirectClipboard))
			return FALSE;
	}

	if (~file->RedirectPrinters)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectPrinters, file->RedirectPrinters))
			return FALSE;
	}

	if (~file->RedirectDrives)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectDrives, file->RedirectDrives))
			return FALSE;
	}

	if (~file->RedirectPosDevices)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectSerialPorts, file->RedirectComPorts) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_RedirectParallelPorts, file->RedirectComPorts))
			return FALSE;
	}

	if (~file->RedirectComPorts)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectSerialPorts, file->RedirectComPorts) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_RedirectParallelPorts, file->RedirectComPorts))
			return FALSE;
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
		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectDrives, TRUE))
			return FALSE;
	}

	if (~((size_t) file->DrivesToRedirect))
	{
		/*
		 * Drives to redirect:
		 *
		 * Very similar to DevicesToRedirect, but can contain a
		 * comma-separated list of drive letters to redirect.
		 */
		const BOOL empty = !file->DrivesToRedirect || (strlen(file->DrivesToRedirect) == 0);

		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectDrives, !empty))
			return FALSE;
	}

	if (~file->KeyboardHook)
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardHook, file->KeyboardHook))
			return FALSE;
	}

	if (~((size_t) file->PreconnectionBlob))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_PreconnectionBlob, file->PreconnectionBlob) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_SendPreconnectionPdu, TRUE) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_VmConnectMode, TRUE))
			return FALSE;
	}

	if (file->argc > 1)
	{
		char* ConnectionFile = settings->ConnectionFile;
		settings->ConnectionFile = NULL;

		if (freerdp_client_settings_parse_command_line(settings, file->argc, file->argv, FALSE) < 0)
			return FALSE;

		settings->ConnectionFile = ConnectionFile;
	}

	return TRUE;
}

static rdpFileLine* freerdp_client_rdp_file_find_line_index(rdpFile* file, int index)
{
	rdpFileLine* line;
	line = &(file->lines[index]);
	return line;
}
static rdpFileLine* freerdp_client_rdp_file_find_line_by_name(rdpFile* file, const char* name)
{
	int index;
	BOOL bFound = FALSE;
	rdpFileLine* line = NULL;

	for (index = 0; index < file->lineCount; index++)
	{
		line = &(file->lines[index]);

		if (line->flags & RDP_FILE_LINE_FLAG_FORMATTED)
		{
			if (strcmp(name, line->name) == 0)
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
	int index;
	int length;
	char* text;
	rdpFileLine* line;
	length = _scprintf("%s:s:%s", name, value);
	text = (char*) malloc(length + 1);

	if (!text)
		return -1;

	sprintf_s(text, length + 1, "%s:s:%s", name, value ? value : "");
	text[length] = '\0';
	line = freerdp_client_rdp_file_find_line_by_name(file, name);

	if (line)
	{
		free(line->sValue);
		line->sValue = _strdup(value);

		if (!line->sValue)
			goto out_fail;

		free(line->text);
		line->text = text;
	}
	else
	{
		index = freerdp_client_parse_rdp_file_add_line(file, text, -1);

		if (index == -1)
			goto out_fail;

		if (!(line = freerdp_client_rdp_file_find_line_index(file, index)))
			goto out_fail;

		if (freerdp_client_rdp_file_set_string(file, name, value, index) == -1)
			goto out_fail;

		free(text);
	}

	return 0;
out_fail:
	free(text);
	return -1;
}
const char* freerdp_client_rdp_file_get_string_option(rdpFile* file, const char* name)
{
	rdpFileLine* line;
	line = freerdp_client_rdp_file_find_line_by_name(file, name);

	if (!line)
		return NULL;

	if (!(line->flags & RDP_FILE_LINE_FLAG_TYPE_STRING))
		return NULL;

	return line->sValue;
}
int freerdp_client_rdp_file_set_integer_option(rdpFile* file, const char* name, int value)
{
	int index;
	const int length = _scprintf("%s:i:%d", name, value);
	char* text = (char*) malloc(length + 1);
	rdpFileLine* line = freerdp_client_rdp_file_find_line_by_name(file, name);

	if (!text)
		return -1;

	sprintf_s(text, length + 1, "%s:i:%d", name, value);
	text[length] = '\0';

	if (line)
	{
		line->iValue = value;
		free(line->text);
		line->text = text;
	}
	else
	{
		index = freerdp_client_parse_rdp_file_add_line(file, text, -1);

		if (index < 0)
		{
			free(text);
			return -1;
		}

		if (!freerdp_client_rdp_file_find_line_index(file, index))
		{
			free(text);
			return -1;
		}

		if (freerdp_client_rdp_file_set_integer(file, (char*) name, value, index) < 0)
		{
			free(text);
			return -1;
		}

		free(text);
	}

	return 0;
}
int freerdp_client_rdp_file_get_integer_option(rdpFile* file, const char* name)
{
	rdpFileLine* line;
	line = freerdp_client_rdp_file_find_line_by_name(file, name);

	if (!line)
		return -1;

	if (!(line->flags & RDP_FILE_LINE_FLAG_TYPE_INTEGER))
		return -1;

	return line->iValue;
}
static void freerdp_client_file_string_check_free(LPSTR str)
{
	if (~((size_t) str))
		free(str);
}
rdpFile* freerdp_client_rdp_file_new()
{
	rdpFile* file;
	file = (rdpFile*) malloc(sizeof(rdpFile));

	if (file)
	{
		FillMemory(file, sizeof(rdpFile), 0xFF);
		file->lineCount = 0;
		file->lineSize = 32;
		file->lines = (rdpFileLine*) calloc(file->lineSize, sizeof(rdpFileLine));

		if (!file->lines)
		{
			free(file);
			return NULL;
		}

		file->argc = 0;
		file->argSize = 32;
		file->argv = (char**) calloc(file->argSize, sizeof(char*));

		if (!file->argv)
		{
			free(file->lines);
			free(file);
			return NULL;
		}

		if (!freerdp_client_add_option(file, "freerdp"))
		{
			free(file->argv);
			free(file->lines);
			free(file);
			return NULL;
		}
	}

	return file;
}
void freerdp_client_rdp_file_free(rdpFile* file)
{
	int i;

	if (file)
	{
		if (file->lineCount)
		{
			for (i = 0; i < file->lineCount; i++)
			{
				free(file->lines[i].text);
				free(file->lines[i].name);
				free(file->lines[i].sValue);
			}

			free(file->lines);
		}

		if (file->argv)
		{
			for (i = 0; i < file->argc; i++)
				free(file->argv[i]);

			free(file->argv);
		}

		freerdp_client_file_string_check_free(file->Username);
		freerdp_client_file_string_check_free(file->Domain);
		freerdp_client_file_string_check_free(file->Password);
		freerdp_client_file_string_check_free(file->FullAddress);
		freerdp_client_file_string_check_free(file->AlternateFullAddress);
		freerdp_client_file_string_check_free(file->UsbDevicesToRedirect);
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
		free(file);
	}
}
