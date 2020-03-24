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
#include <ctype.h>

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
#include <winpr/path.h>
#include <freerdp/log.h>
#define TAG CLIENT_TAG("common")

/*#define DEBUG_CLIENT_FILE	1*/

static const BYTE BOM_UTF16_LE[2] = { 0xFF, 0xFE };

#define INVALID_INTEGER_VALUE 0xFFFFFFFF

#define RDP_FILE_LINE_FLAG_FORMATTED 0x00000001
#define RDP_FILE_LINE_FLAG_STANDARD 0x00000002
#define RDP_FILE_LINE_FLAG_TYPE_STRING 0x00000010
#define RDP_FILE_LINE_FLAG_TYPE_INTEGER 0x00000020
#define RDP_FILE_LINE_FLAG_TYPE_BINARY 0x00000040

struct rdp_file_line
{
	char* text;
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
	DWORD ScreenModeId;                /* screen mode id */
	DWORD SpanMonitors;                /* span monitors */
	DWORD SmartSizing;                 /* smartsizing */
	DWORD EnableSuperSpan;             /* enablesuperpan */
	DWORD SuperSpanAccelerationFactor; /* superpanaccelerationfactor */

	DWORD DesktopWidth;  /* desktopwidth */
	DWORD DesktopHeight; /* desktopheight */
	DWORD DesktopSizeId; /* desktop size id */
	DWORD SessionBpp;    /* session bpp */

	DWORD Compression;       /* compression */
	DWORD KeyboardHook;      /* keyboardhook */
	DWORD DisableCtrlAltDel; /* disable ctrl+alt+del */

	DWORD AudioMode;         /* audiomode */
	DWORD AudioQualityMode;  /* audioqualitymode */
	DWORD AudioCaptureMode;  /* audiocapturemode */
	DWORD VideoPlaybackMode; /* videoplaybackmode */

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
	DWORD RedirectSmartCards;          /* redirectsmartcards */
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

	size_t argc;
	char** argv;
	size_t argSize;
	void* context;

	DWORD flags;
};

/*
 * Set an integer in a rdpFile
 *
 * @return FALSE if a standard name was set, TRUE for a non-standard name, FALSE on error
 *
 */

static BOOL freerdp_client_rdp_file_set_integer(rdpFile* file, const char* name, long value,
                                                SSIZE_T index)
{
	BOOL standard = TRUE;
#ifdef DEBUG_CLIENT_FILE
	WLog_DBG(TAG, "%s:i:%d", name, value);
#endif

	if (value < 0)
		return FALSE;

	if (_stricmp(name, "use multimon") == 0)
		file->UseMultiMon = (UINT32)value;
	else if (_stricmp(name, "screen mode id") == 0)
		file->ScreenModeId = (UINT32)value;
	else if (_stricmp(name, "span monitors") == 0)
		file->SpanMonitors = (UINT32)value;
	else if (_stricmp(name, "smart sizing") == 0)
		file->SmartSizing = (UINT32)value;
	else if (_stricmp(name, "enablesuperpan") == 0)
		file->EnableSuperSpan = (UINT32)value;
	else if (_stricmp(name, "superpanaccelerationfactor") == 0)
		file->SuperSpanAccelerationFactor = (UINT32)value;
	else if (_stricmp(name, "desktopwidth") == 0)
		file->DesktopWidth = (UINT32)value;
	else if (_stricmp(name, "desktopheight") == 0)
		file->DesktopHeight = (UINT32)value;
	else if (_stricmp(name, "desktop size id") == 0)
		file->DesktopSizeId = (UINT32)value;
	else if (_stricmp(name, "session bpp") == 0)
		file->SessionBpp = (UINT32)value;
	else if (_stricmp(name, "compression") == 0)
		file->Compression = (UINT32)value;
	else if (_stricmp(name, "keyboardhook") == 0)
		file->KeyboardHook = (UINT32)value;
	else if (_stricmp(name, "disable ctrl+alt+del") == 0)
		file->DisableCtrlAltDel = (UINT32)value;
	else if (_stricmp(name, "audiomode") == 0)
		file->AudioMode = (UINT32)value;
	else if (_stricmp(name, "audioqualitymode") == 0)
		file->AudioQualityMode = (UINT32)value;
	else if (_stricmp(name, "audiocapturemode") == 0)
		file->AudioCaptureMode = (UINT32)value;
	else if (_stricmp(name, "videoplaybackmode") == 0)
		file->VideoPlaybackMode = (UINT32)value;
	else if (_stricmp(name, "connection type") == 0)
		file->ConnectionType = (UINT32)value;
	else if (_stricmp(name, "networkautodetect") == 0)
		file->NetworkAutoDetect = (UINT32)value;
	else if (_stricmp(name, "bandwidthautodetect") == 0)
		file->BandwidthAutoDetect = (UINT32)value;
	else if (_stricmp(name, "pinconnectionbar") == 0)
		file->PinConnectionBar = (UINT32)value;
	else if (_stricmp(name, "displayconnectionbar") == 0)
		file->DisplayConnectionBar = (UINT32)value;
	else if (_stricmp(name, "workspaceid") == 0)
		file->WorkspaceId = (UINT32)value;
	else if (_stricmp(name, "enableworkspacereconnect") == 0)
		file->EnableWorkspaceReconnect = (UINT32)value;
	else if (_stricmp(name, "disable wallpaper") == 0)
		file->DisableWallpaper = (UINT32)value;
	else if (_stricmp(name, "allow font smoothing") == 0)
		file->AllowFontSmoothing = (UINT32)value;
	else if (_stricmp(name, "allow desktop composition") == 0)
		file->AllowDesktopComposition = (UINT32)value;
	else if (_stricmp(name, "disable full window drag") == 0)
		file->DisableFullWindowDrag = (UINT32)value;
	else if (_stricmp(name, "disable menu anims") == 0)
		file->DisableMenuAnims = (UINT32)value;
	else if (_stricmp(name, "disable themes") == 0)
		file->DisableThemes = (UINT32)value;
	else if (_stricmp(name, "disable cursor setting") == 0)
		file->DisableCursorSetting = (UINT32)value;
	else if (_stricmp(name, "bitmapcachesize") == 0)
		file->BitmapCacheSize = (UINT32)value;
	else if (_stricmp(name, "bitmapcachepersistenable") == 0)
		file->BitmapCachePersistEnable = (UINT32)value;
	else if (_stricmp(name, "server port") == 0)
		file->ServerPort = (UINT32)value;
	else if (_stricmp(name, "redirectdrives") == 0)
		file->RedirectDrives = (UINT32)value;
	else if (_stricmp(name, "redirectprinters") == 0)
		file->RedirectPrinters = (UINT32)value;
	else if (_stricmp(name, "redirectcomports") == 0)
		file->RedirectComPorts = (UINT32)value;
	else if (_stricmp(name, "redirectsmartcards") == 0)
		file->RedirectSmartCards = (UINT32)value;
	else if (_stricmp(name, "redirectclipboard") == 0)
		file->RedirectClipboard = (UINT32)value;
	else if (_stricmp(name, "redirectposdevices") == 0)
		file->RedirectPosDevices = (UINT32)value;
	else if (_stricmp(name, "redirectdirectx") == 0)
		file->RedirectDirectX = (UINT32)value;
	else if (_stricmp(name, "disableprinterredirection") == 0)
		file->DisablePrinterRedirection = (UINT32)value;
	else if (_stricmp(name, "disableclipboardredirection") == 0)
		file->DisableClipboardRedirection = (UINT32)value;
	else if (_stricmp(name, "connect to console") == 0)
		file->ConnectToConsole = (UINT32)value;
	else if (_stricmp(name, "administrative session") == 0)
		file->AdministrativeSession = (UINT32)value;
	else if (_stricmp(name, "autoreconnection enabled") == 0)
		file->AutoReconnectionEnabled = (UINT32)value;
	else if (_stricmp(name, "autoreconnect max retries") == 0)
		file->AutoReconnectMaxRetries = (UINT32)value;
	else if (_stricmp(name, "public mode") == 0)
		file->PublicMode = (UINT32)value;
	else if (_stricmp(name, "authentication level") == 0)
		file->AuthenticationLevel = (UINT32)value;
	else if (_stricmp(name, "promptcredentialonce") == 0)
		file->PromptCredentialOnce = (UINT32)value;
	else if ((_stricmp(name, "prompt for credentials") == 0))
		file->PromptForCredentials = (UINT32)value;
	else if (_stricmp(name, "negotiate security layer") == 0)
		file->NegotiateSecurityLayer = (UINT32)value;
	else if (_stricmp(name, "enablecredsspsupport") == 0)
		file->EnableCredSSPSupport = (UINT32)value;
	else if (_stricmp(name, "remoteapplicationmode") == 0)
		file->RemoteApplicationMode = (UINT32)value;
	else if (_stricmp(name, "remoteapplicationexpandcmdline") == 0)
		file->RemoteApplicationExpandCmdLine = (UINT32)value;
	else if (_stricmp(name, "remoteapplicationexpandworkingdir") == 0)
		file->RemoteApplicationExpandWorkingDir = (UINT32)value;
	else if (_stricmp(name, "disableconnectionsharing") == 0)
		file->DisableConnectionSharing = (UINT32)value;
	else if (_stricmp(name, "disableremoteappcapscheck") == 0)
		file->DisableRemoteAppCapsCheck = (UINT32)value;
	else if (_stricmp(name, "gatewayusagemethod") == 0)
		file->GatewayUsageMethod = (UINT32)value;
	else if (_stricmp(name, "gatewayprofileusagemethod") == 0)
		file->GatewayProfileUsageMethod = (UINT32)value;
	else if (_stricmp(name, "gatewaycredentialssource") == 0)
		file->GatewayCredentialsSource = (UINT32)value;
	else if (_stricmp(name, "use redirection server name") == 0)
		file->UseRedirectionServerName = (UINT32)value;
	else if (_stricmp(name, "rdgiskdcproxy") == 0)
		file->RdgIsKdcProxy = (UINT32)value;
	else
		standard = FALSE;

	if (index >= 0)
	{
		file->lines[index].name = _strdup(name);

		if (!file->lines[index].name)
			return FALSE;

		file->lines[index].iValue = value;
		file->lines[index].flags = RDP_FILE_LINE_FLAG_FORMATTED;
		file->lines[index].flags |= RDP_FILE_LINE_FLAG_TYPE_INTEGER;

		if (standard)
			file->lines[index].flags |= RDP_FILE_LINE_FLAG_STANDARD;

		file->lines[index].valueLength = 0;
	}

	return !standard;
}

static BOOL freerdp_client_parse_rdp_file_integer(rdpFile* file, const char* name,
                                                  const char* value, SSIZE_T index)
{
	char* endptr;
	long ivalue;
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

	freerdp_client_rdp_file_set_integer(file, name, ivalue, index);
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
                                              SSIZE_T index)
{
	int standard = 0;
	LPSTR* tmp = NULL;
#ifdef DEBUG_CLIENT_FILE
	WLog_DBG(TAG, "%s:s:%s", name, value);
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

static BOOL freerdp_client_add_option(rdpFile* file, const char* option)
{
	while ((file->argc + 1) > file->argSize)
	{
		size_t new_size;
		char** new_argv;
		new_size = file->argSize * 2;
		new_argv = (char**)realloc(file->argv, new_size * sizeof(char*));

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

static SSIZE_T freerdp_client_parse_rdp_file_add_line(rdpFile* file, const char* line,
                                                      SSIZE_T index)
{
	if (index < 0)
		index = (SSIZE_T)file->lineCount;

	while ((file->lineCount + 1) > file->lineSize)
	{
		size_t new_size;
		rdpFileLine* new_line;
		new_size = file->lineSize * 2;
		new_line = (rdpFileLine*)realloc(file->lines, new_size * sizeof(rdpFileLine));

		if (!new_line)
			return -1;

		file->lines = new_line;
		file->lineSize = new_size;
	}

	ZeroMemory(&(file->lines[file->lineCount]), sizeof(rdpFileLine));
	file->lines[file->lineCount].text = _strdup(line);

	if (!file->lines[file->lineCount].text)
		return -1;

	file->lines[file->lineCount].index = (size_t)index;
	(file->lineCount)++;
	return index;
}

static BOOL freerdp_client_parse_rdp_file_string(rdpFile* file, char* name, char* value,
                                                 SSIZE_T index)
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

static BOOL freerdp_client_parse_rdp_file_option(rdpFile* file, const char* option, SSIZE_T index)
{
	WINPR_UNUSED(index);
	return freerdp_client_add_option(file, option);
}

BOOL freerdp_client_parse_rdp_file_buffer(rdpFile* file, const BYTE* buffer, size_t size)
{
	return freerdp_client_parse_rdp_file_buffer_ex(file, buffer, size, NULL);
}

static BOOL trim(char** strptr)
{
	char* start;
	char* str;
	char* end;

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
	return TRUE;
}

BOOL freerdp_client_parse_rdp_file_buffer_ex(rdpFile* file, const BYTE* buffer, size_t size,
                                             rdp_file_fkt_parse parse)
{
	BOOL rc = FALSE;
	SSIZE_T index;
	size_t length;
	char* line;
	char* type;
	char* context;
	char *d1, *d2;
	char* beg;
	char *name, *value;
	char* copy = NULL;

	if (size < 2)
		return FALSE;

	if ((buffer[0] == BOM_UTF16_LE[0]) && (buffer[1] == BOM_UTF16_LE[1]))
	{
		int length;
		LPCWSTR uc = (LPCWSTR)(&buffer[2]);
		size = size / 2 - 1;

		if (size > INT_MAX)
			return FALSE;

		length = (int)size;
		if (ConvertFromUnicode(CP_UTF8, 0, uc, length, &copy, 0, NULL, NULL) < 0)
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

			if (parse && parse(file->context, name, *type, value))
			{
			}
			else if (*type == 'i')
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

	buffer = (BYTE*)malloc((size_t)file_size + 2);

	if (!buffer)
	{
		fclose(fp);
		return FALSE;
	}

	read_size = fread(buffer, (size_t)file_size, 1, fp);

	if (!read_size)
	{
		if (!ferror(fp))
			read_size = (size_t)file_size;
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
	status = freerdp_client_parse_rdp_file_buffer_ex(file, buffer, (size_t)file_size, parse);
	free(buffer);
	return status;
}

#define FILE_POPULATE_STRING(_target, _setting) \
	do                                          \
	{                                           \
		if (_setting)                           \
		{                                       \
			_target = _strdup(_setting);        \
			if (!_target)                       \
				return FALSE;                   \
		}                                       \
	} while (0)

BOOL freerdp_client_populate_rdp_file_from_settings(rdpFile* file, const rdpSettings* settings)
{
	FILE_POPULATE_STRING(file->Domain, settings->Domain);
	FILE_POPULATE_STRING(file->Username, settings->Username);
	FILE_POPULATE_STRING(file->Password, settings->Password);
	file->ServerPort = settings->ServerPort;
	FILE_POPULATE_STRING(file->FullAddress, settings->ServerHostname);
	FILE_POPULATE_STRING(file->AlternateFullAddress, settings->ServerHostname);
	file->DesktopWidth = settings->DesktopWidth;
	file->DesktopHeight = settings->DesktopHeight;
	file->SessionBpp = settings->ColorDepth;
	file->ConnectToConsole = settings->ConsoleSession;
	file->NegotiateSecurityLayer = settings->NegotiateSecurityLayer;
	file->EnableCredSSPSupport = settings->NlaSecurity;
	FILE_POPULATE_STRING(file->AlternateShell, settings->AlternateShell);
	FILE_POPULATE_STRING(file->ShellWorkingDirectory, settings->ShellWorkingDirectory);
	file->ConnectionType = settings->ConnectionType;
	FILE_POPULATE_STRING(file->DrivesToRedirect, settings->DrivesToRedirect);
	file->ScreenModeId = settings->Fullscreen ? 2 : 1;

	if (settings->LoadBalanceInfoLength)
	{
		file->LoadBalanceInfo = calloc(settings->LoadBalanceInfoLength + 1, 1);
		if (!file->LoadBalanceInfo)
			return FALSE;
		memcpy(file->LoadBalanceInfo, settings->LoadBalanceInfo, settings->LoadBalanceInfoLength);
	}

	if (settings->AudioPlayback)
		file->AudioMode = AUDIO_MODE_REDIRECT;
	else if (settings->RemoteConsoleAudio)
		file->AudioMode = AUDIO_MODE_PLAY_ON_SERVER;
	else
		file->AudioMode = AUDIO_MODE_NONE;

	file->AudioCaptureMode = settings->AudioCapture;
	file->Compression = settings->CompressionEnabled;
	FILE_POPULATE_STRING(file->GatewayHostname, settings->GatewayHostname);
	FILE_POPULATE_STRING(file->GatewayAccessToken, settings->GatewayAccessToken);
	file->GatewayUsageMethod = settings->GatewayUsageMethod;
	file->PromptCredentialOnce = settings->GatewayUseSameCredentials;
	file->PromptForCredentials = settings->PromptForCredentials;
	file->RemoteApplicationMode = settings->RemoteApplicationMode;
	FILE_POPULATE_STRING(file->RemoteApplicationProgram, settings->RemoteApplicationProgram);
	FILE_POPULATE_STRING(file->RemoteApplicationName, settings->RemoteApplicationName);
	FILE_POPULATE_STRING(file->RemoteApplicationIcon, settings->RemoteApplicationIcon);
	FILE_POPULATE_STRING(file->RemoteApplicationFile, settings->RemoteApplicationFile);
	FILE_POPULATE_STRING(file->RemoteApplicationGuid, settings->RemoteApplicationGuid);
	FILE_POPULATE_STRING(file->RemoteApplicationCmdLine, settings->RemoteApplicationCmdLine);
	file->SpanMonitors = settings->SpanMonitors;
	file->UseMultiMon = settings->UseMultimon;
	file->AllowFontSmoothing = settings->AllowFontSmoothing;
	file->DisableWallpaper = settings->DisableWallpaper;
	file->DisableFullWindowDrag = settings->DisableFullWindowDrag;
	file->DisableMenuAnims = settings->DisableMenuAnims;
	file->DisableThemes = settings->DisableThemes;
	file->BandwidthAutoDetect = (settings->ConnectionType >= 7) ? TRUE : FALSE;
	file->NetworkAutoDetect = settings->NetworkAutoDetect;
	file->AutoReconnectionEnabled = settings->AutoReconnectionEnabled;
	file->RedirectSmartCards = settings->RedirectSmartCards;
	file->RedirectClipboard = settings->RedirectClipboard;
	file->RedirectPrinters = settings->RedirectPrinters;
	file->RedirectDrives = settings->RedirectDrives;
	file->RedirectComPorts = (settings->RedirectSerialPorts || settings->RedirectParallelPorts);
	FILE_POPULATE_STRING(file->DrivesToRedirect, settings->DrivesToRedirect);
	file->KeyboardHook = settings->KeyboardHook;
	FILE_POPULATE_STRING(file->PreconnectionBlob, settings->PreconnectionBlob);

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
	if (size == 0)
		return FALSE;
	buffer = (char*)calloc((size_t)(size + 1), sizeof(char));

	if (freerdp_client_write_rdp_file_buffer(file, buffer, (size_t)size + 1) != size)
	{
		WLog_ERR(TAG, "freerdp_client_write_rdp_file: error writing to output buffer");
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
			if ((length < 0) || (fwrite(BOM_UTF16_LE, sizeof(BYTE), 2, fp) != 2) ||
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
			if (fwrite(buffer, 1, (size_t)size, fp) != (size_t)size)
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
static SSIZE_T freerdp_client_write_setting_to_buffer(char** buffer, size_t* bufferSize,
                                                      const char* fmt, ...)
{
	va_list ap;
	SSIZE_T len;
	char* buf;
	size_t bufSize;

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
#pragma GCC diagnostic pop

size_t freerdp_client_write_rdp_file_buffer(const rdpFile* file, char* buffer, size_t size)
{
	size_t totalSize = 0;

	/* either buffer and size are null or non-null */
	if ((!buffer || !size) && (buffer || size))
		return 0;

#define WRITE_SETTING_(fmt_, param_)                                                        \
	{                                                                                       \
		SSIZE_T res = freerdp_client_write_setting_to_buffer(&buffer, &size, fmt_, param_); \
		if (res < 0)                                                                        \
			return 0;                                                                       \
		totalSize += (size_t)res;                                                           \
	}

#define WRITE_SETTING_INT(fmt_, param_) \
	if (~(param_))                      \
	WRITE_SETTING_(fmt_, param_)

#define WRITE_SETTING_STR(fmt_, param_) \
	if (~(size_t)(param_))              \
	WRITE_SETTING_(fmt_, param_)

	/* integer parameters */
	WRITE_SETTING_INT("use multimon:i:%" PRIu32, file->UseMultiMon);
	WRITE_SETTING_INT("screen mode id:i:%" PRIu32, file->ScreenModeId);
	WRITE_SETTING_INT("span monitors:i:%" PRIu32, file->SpanMonitors);
	WRITE_SETTING_INT("smart sizing:i:%" PRIu32, file->SmartSizing);
	WRITE_SETTING_INT("enablesuperpan:i:%" PRIu32, file->EnableSuperSpan);
	WRITE_SETTING_INT("superpanaccelerationfactor:i:%" PRIu32, file->SuperSpanAccelerationFactor);
	WRITE_SETTING_INT("desktopwidth:i:%" PRIu32, file->DesktopWidth);
	WRITE_SETTING_INT("desktopheight:i:%" PRIu32, file->DesktopHeight);
	WRITE_SETTING_INT("desktop size id:i:%" PRIu32, file->DesktopSizeId);
	WRITE_SETTING_INT("session bpp:i:%" PRIu32, file->SessionBpp);
	WRITE_SETTING_INT("compression:i:%" PRIu32, file->Compression);
	WRITE_SETTING_INT("keyboardhook:i:%" PRIu32, file->KeyboardHook);
	WRITE_SETTING_INT("disable ctrl+alt+del:i:%" PRIu32, file->DisableCtrlAltDel);
	WRITE_SETTING_INT("audiomode:i:%" PRIu32, file->AudioMode);
	WRITE_SETTING_INT("audioqualitymode:i:%" PRIu32, file->AudioQualityMode);
	WRITE_SETTING_INT("audiocapturemode:i:%" PRIu32, file->AudioCaptureMode);
	WRITE_SETTING_INT("videoplaybackmode:i:%" PRIu32, file->VideoPlaybackMode);
	WRITE_SETTING_INT("connection type:i:%" PRIu32, file->ConnectionType);
	WRITE_SETTING_INT("networkautodetect:i:%" PRIu32, file->NetworkAutoDetect);
	WRITE_SETTING_INT("bandwidthautodetect:i:%" PRIu32, file->BandwidthAutoDetect);
	WRITE_SETTING_INT("pinconnectionbar:i:%" PRIu32, file->PinConnectionBar);
	WRITE_SETTING_INT("displayconnectionbar:i:%" PRIu32, file->DisplayConnectionBar);
	WRITE_SETTING_INT("workspaceid:i:%" PRIu32, file->WorkspaceId);
	WRITE_SETTING_INT("enableworkspacereconnect:i:%" PRIu32, file->EnableWorkspaceReconnect);
	WRITE_SETTING_INT("disable wallpaper:i:%" PRIu32, file->DisableWallpaper);
	WRITE_SETTING_INT("allow font smoothing:i:%" PRIu32, file->AllowFontSmoothing);
	WRITE_SETTING_INT("allow desktop composition:i:%" PRIu32, file->AllowDesktopComposition);
	WRITE_SETTING_INT("disable full window drag:i:%" PRIu32, file->DisableFullWindowDrag);
	WRITE_SETTING_INT("disable menu anims:i:%" PRIu32, file->DisableMenuAnims);
	WRITE_SETTING_INT("disable themes:i:%" PRIu32, file->DisableThemes);
	WRITE_SETTING_INT("disable cursor setting:i:%" PRIu32, file->DisableCursorSetting);
	WRITE_SETTING_INT("bitmapcachesize:i:%" PRIu32, file->BitmapCacheSize);
	WRITE_SETTING_INT("bitmapcachepersistenable:i:%" PRIu32, file->BitmapCachePersistEnable);
	WRITE_SETTING_INT("server port:i:%" PRIu32, file->ServerPort);
	WRITE_SETTING_INT("redirectdrives:i:%" PRIu32, file->RedirectDrives);
	WRITE_SETTING_INT("redirectprinters:i:%" PRIu32, file->RedirectPrinters);
	WRITE_SETTING_INT("redirectcomports:i:%" PRIu32, file->RedirectComPorts);
	WRITE_SETTING_INT("redirectsmartcards:i:%" PRIu32, file->RedirectSmartCards);
	WRITE_SETTING_INT("redirectclipboard:i:%" PRIu32, file->RedirectClipboard);
	WRITE_SETTING_INT("redirectposdevices:i:%" PRIu32, file->RedirectPosDevices);
	WRITE_SETTING_INT("redirectdirectx:i:%" PRIu32, file->RedirectDirectX);
	WRITE_SETTING_INT("disableprinterredirection:i:%" PRIu32, file->DisablePrinterRedirection);
	WRITE_SETTING_INT("disableclipboardredirection:i:%" PRIu32, file->DisableClipboardRedirection);
	WRITE_SETTING_INT("connect to console:i:%" PRIu32, file->ConnectToConsole);
	WRITE_SETTING_INT("administrative session:i:%" PRIu32, file->AdministrativeSession);
	WRITE_SETTING_INT("autoreconnection enabled:i:%" PRIu32, file->AutoReconnectionEnabled);
	WRITE_SETTING_INT("autoreconnect max retries:i:%" PRIu32, file->AutoReconnectMaxRetries);
	WRITE_SETTING_INT("public mode:i:%" PRIu32, file->PublicMode);
	WRITE_SETTING_INT("authentication level:i:%" PRId32, file->AuthenticationLevel);
	WRITE_SETTING_INT("promptcredentialonce:i:%" PRIu32, file->PromptCredentialOnce);
	WRITE_SETTING_INT("prompt for credentials:i:%" PRIu32, file->PromptForCredentials);
	WRITE_SETTING_INT("negotiate security layer:i:%" PRIu32, file->NegotiateSecurityLayer);
	WRITE_SETTING_INT("enablecredsspsupport:i:%" PRIu32, file->EnableCredSSPSupport);
	WRITE_SETTING_INT("remoteapplicationmode:i:%" PRIu32, file->RemoteApplicationMode);
	WRITE_SETTING_INT("remoteapplicationexpandcmdline:i:%" PRIu32,
	                  file->RemoteApplicationExpandCmdLine);
	WRITE_SETTING_INT("remoteapplicationexpandworkingdir:i:%" PRIu32,
	                  file->RemoteApplicationExpandWorkingDir);
	WRITE_SETTING_INT("disableconnectionsharing:i:%" PRIu32, file->DisableConnectionSharing);
	WRITE_SETTING_INT("disableremoteappcapscheck:i:%" PRIu32, file->DisableRemoteAppCapsCheck);
	WRITE_SETTING_INT("gatewayusagemethod:i:%" PRIu32, file->GatewayUsageMethod);
	WRITE_SETTING_INT("gatewayprofileusagemethod:i:%" PRIu32, file->GatewayProfileUsageMethod);
	WRITE_SETTING_INT("gatewaycredentialssource:i:%" PRIu32, file->GatewayCredentialsSource);
	WRITE_SETTING_INT("use redirection server name:i:%" PRIu32, file->UseRedirectionServerName);
	WRITE_SETTING_INT("rdgiskdcproxy:i:%" PRIu32, file->RdgIsKdcProxy);

	/* string parameters */
	WRITE_SETTING_STR("username:s:%s", file->Username);
	WRITE_SETTING_STR("domain:s:%s", file->Domain);
	WRITE_SETTING_STR("password:s:%s", file->Password);
	WRITE_SETTING_STR("full address:s:%s", file->FullAddress);
	WRITE_SETTING_STR("alternate full address:s:%s", file->AlternateFullAddress);
	WRITE_SETTING_STR("usbdevicestoredirect:s:%s", file->UsbDevicesToRedirect);
	WRITE_SETTING_STR("loadbalanceinfo:s:%s", file->LoadBalanceInfo);
	WRITE_SETTING_STR("remoteapplicationname:s:%s", file->RemoteApplicationName);
	WRITE_SETTING_STR("remoteapplicationicon:s:%s", file->RemoteApplicationIcon);
	WRITE_SETTING_STR("remoteapplicationprogram:s:%s", file->RemoteApplicationProgram);
	WRITE_SETTING_STR("remoteapplicationfile:s:%s", file->RemoteApplicationFile);
	WRITE_SETTING_STR("remoteapplicationguid:s:%s", file->RemoteApplicationGuid);
	WRITE_SETTING_STR("remoteapplicationcmdline:s:%s", file->RemoteApplicationCmdLine);
	WRITE_SETTING_STR("alternate shell:s:%s", file->AlternateShell);
	WRITE_SETTING_STR("shell working directory:s:%s", file->ShellWorkingDirectory);
	WRITE_SETTING_STR("gatewayhostname:s:%s", file->GatewayHostname);
	WRITE_SETTING_STR("gatewayaccesstoken:s:%s", file->GatewayAccessToken);
	WRITE_SETTING_STR("kdcproxyname:s:%s", file->KdcProxyName);
	WRITE_SETTING_STR("drivestoredirect:s:%s", file->DrivesToRedirect);
	WRITE_SETTING_STR("devicestoredirect:s:%s", file->DevicesToRedirect);
	WRITE_SETTING_STR("winposstr:s:%s", file->WinPosStr);
	WRITE_SETTING_STR("pcb:s:%s", file->PreconnectionBlob);

	return totalSize;
}

BOOL freerdp_client_populate_settings_from_rdp_file(rdpFile* file, rdpSettings* settings)
{
	BOOL setDefaultConnectionType = TRUE;

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

			if (!freerdp_settings_set_string(settings, FreeRDP_ServerHostname, host))
				return FALSE;

			free(host);

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

	if (~((size_t)file->AlternateShell))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_AlternateShell, file->AlternateShell))
			return FALSE;
	}

	if (~((size_t)file->ShellWorkingDirectory))
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
		settings->LoadBalanceInfo = (BYTE*)_strdup(file->LoadBalanceInfo);

		if (!settings->LoadBalanceInfo)
			return FALSE;

		settings->LoadBalanceInfoLength = (UINT32)strlen((char*)settings->LoadBalanceInfo);
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
		settings->AuthenticationLevel = file->AuthenticationLevel;
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

		if (!freerdp_settings_set_string(settings, FreeRDP_GatewayHostname, host))
			return FALSE;

		free(host);

		if (port > 0)
		{
			if (!freerdp_settings_set_uint32(settings, FreeRDP_GatewayPort, (UINT32)port))
				return FALSE;
		}
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
			if (!freerdp_set_connection_type(settings, CONNECTION_TYPE_AUTODETECT))
				return FALSE;
			setDefaultConnectionType = FALSE;
		}
		if (!freerdp_settings_set_bool(settings, FreeRDP_NetworkAutoDetect,
		                               file->BandwidthAutoDetect != 0))
			return FALSE;
	}

	if (~file->NetworkAutoDetect)
	{
		if (file->NetworkAutoDetect == 0)
		{
			if (file->BandwidthAutoDetect != CONNECTION_TYPE_AUTODETECT)
			{
				WLog_WARN(TAG,
				          "Got networkautodetect:i:%" PRIu32 " and bandwidthautodetect:i:%" PRIu32
				          ". Correcting to bandwidthautodetect:i:%" PRIu32,
				          file->NetworkAutoDetect, file->BandwidthAutoDetect,
				          CONNECTION_TYPE_AUTODETECT);
				WLog_WARN(TAG,
				          "Add bandwidthautodetect:i:%" PRIu32
				          " to your RDP file to eliminate this warning.",
				          CONNECTION_TYPE_AUTODETECT);
			}

			if (!freerdp_set_connection_type(settings, CONNECTION_TYPE_AUTODETECT))
				return FALSE;
			setDefaultConnectionType = FALSE;
		}
		if (!freerdp_settings_set_bool(settings, FreeRDP_NetworkAutoDetect,
		                               file->NetworkAutoDetect == 0))
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

	if (~file->RedirectDirectX)
	{
		/* What is this?! */
	}

	if (~((size_t)file->DevicesToRedirect))
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

	if (~((size_t)file->DrivesToRedirect))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_DrivesToRedirect,
		                                 file->DrivesToRedirect))
			return FALSE;
	}

	if (~file->KeyboardHook)
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardHook, file->KeyboardHook))
			return FALSE;
	}

	if (~((size_t)file->PreconnectionBlob))
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_PreconnectionBlob,
		                                 file->PreconnectionBlob) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_SendPreconnectionPdu, TRUE) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_VmConnectMode, TRUE))
			return FALSE;
	}

	if (file->argc > 1)
	{
		char* ConnectionFile = settings->ConnectionFile;
		settings->ConnectionFile = NULL;

		if (freerdp_client_settings_parse_command_line(settings, (int)file->argc, file->argv,
		                                               FALSE) < 0)
			return FALSE;

		settings->ConnectionFile = ConnectionFile;
	}

	if (setDefaultConnectionType)
	{
		if (!freerdp_set_connection_type(settings, CONNECTION_TYPE_AUTODETECT))
			return FALSE;
	}

	return TRUE;
}

static rdpFileLine* freerdp_client_rdp_file_find_line_index(rdpFile* file, SSIZE_T index)
{
	rdpFileLine* line;
	if (index < 0)
		return NULL;
	line = &(file->lines[index]);
	return line;
}

static rdpFileLine* freerdp_client_rdp_file_find_line_by_name(rdpFile* file, const char* name)
{
	size_t index;
	BOOL bFound = FALSE;
	rdpFileLine* line = NULL;

	for (index = 0; index < file->lineCount; index++)
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
	int length;
	char* text;
	rdpFileLine* line;
	length = _scprintf("%s:s:%s", name, value);
	if (length < 0)
		return -1;
	text = (char*)malloc((size_t)length + 1);

	if (!text)
		return -1;

	sprintf_s(text, (size_t)length + 1, "%s:s:%s", name, value ? value : "");
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
		SSIZE_T index = freerdp_client_parse_rdp_file_add_line(file, text, -1);

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
	SSIZE_T index;
	char* text;
	rdpFileLine* line;
	const int length = _scprintf("%s:i:%d", name, value);

	if (length < 0)
		return -1;

	text = (char*)malloc((size_t)length + 1);
	line = freerdp_client_rdp_file_find_line_by_name(file, name);

	if (!text)
		return -1;

	sprintf_s(text, (size_t)length + 1, "%s:i:%d", name, value);
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

		freerdp_client_rdp_file_set_integer(file, name, value, index);
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

	return (int)line->iValue;
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
	rdpFile* file = (rdpFile*)malloc(sizeof(rdpFile));

	if (!file)
		return NULL;

	file->flags = flags;

	FillMemory(file, sizeof(rdpFile), 0xFF);
	file->argv = NULL;
	file->lines = NULL;
	file->lineCount = 0;
	file->lineSize = 32;
	file->lines = (rdpFileLine*)calloc(file->lineSize, sizeof(rdpFileLine));

	if (!file->lines)
		goto fail;

	file->argc = 0;
	file->argSize = 32;
	file->argv = (char**)calloc(file->argSize, sizeof(char*));

	if (!file->argv)
		goto fail;

	if (!freerdp_client_add_option(file, "freerdp"))
		goto fail;

	return file;
fail:
	freerdp_client_rdp_file_free(file);
	return NULL;
}
void freerdp_client_rdp_file_free(rdpFile* file)
{
	if (file)
	{
		if (file->lineCount)
		{
			size_t i;
			for (i = 0; i < file->lineCount; i++)
			{
				free(file->lines[i].text);
				free(file->lines[i].name);
				free(file->lines[i].sValue);
			}
		}
		free(file->lines);

		if (file->argv)
		{
			size_t i;
			for (i = 0; i < file->argc; i++)
				free(file->argv[i]);
		}
		free(file->argv);

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

void freerdp_client_rdp_file_set_callback_context(rdpFile* file, void* context)
{
	file->context = context;
}
