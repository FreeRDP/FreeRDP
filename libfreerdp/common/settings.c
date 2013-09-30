/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Settings Management
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#include <freerdp/settings.h>
#include <freerdp/freerdp.h>

int freerdp_addin_set_argument(ADDIN_ARGV* args, char* argument)
{
	int i;

	for (i = 0; i < args->argc; i++)
	{
		if (strcmp(args->argv[i], argument) == 0)
		{
			return 1;
		}
	}

	args->argc++;
	args->argv = (char**) realloc(args->argv, sizeof(char*) * args->argc);
	args->argv[args->argc - 1] = _strdup(argument);

	return 0;
}

int freerdp_addin_replace_argument(ADDIN_ARGV* args, char* previous, char* argument)
{
	int i;

	for (i = 0; i < args->argc; i++)
	{
		if (strcmp(args->argv[i], previous) == 0)
		{
			free(args->argv[i]);
			args->argv[i] = _strdup(argument);

			return 1;
		}
	}

	args->argc++;
	args->argv = (char**) realloc(args->argv, sizeof(char*) * args->argc);
	args->argv[args->argc - 1] = _strdup(argument);

	return 0;
}

int freerdp_addin_set_argument_value(ADDIN_ARGV* args, char* option, char* value)
{
	int i;
	char* p;
	char* str;
	int length;

	length = strlen(option) + strlen(value) + 1;
	str = (char*) malloc(length + 1);
	sprintf_s(str, length + 1, "%s:%s", option, value);

	for (i = 0; i < args->argc; i++)
	{
		p = strchr(args->argv[i], ':');

		if (p)
		{
			if (strncmp(args->argv[i], option, p - args->argv[i]) == 0)
			{
				free(args->argv[i]);
				args->argv[i] = str;

				return 1;
			}
		}
	}

	args->argc++;
	args->argv = (char**) realloc(args->argv, sizeof(char*) * args->argc);
	args->argv[args->argc - 1] = str;

	return 0;
}

int freerdp_addin_replace_argument_value(ADDIN_ARGV* args, char* previous, char* option, char* value)
{
	int i;
	char* str;
	int length;

	length = strlen(option) + strlen(value) + 1;
	str = (char*) malloc(length + 1);
	sprintf_s(str, length + 1, "%s:%s", option, value);

	for (i = 0; i < args->argc; i++)
	{
		if (strcmp(args->argv[i], previous) == 0)
		{
			free(args->argv[i]);
			args->argv[i] = str;

			return 1;
		}
	}

	args->argc++;
	args->argv = (char**) realloc(args->argv, sizeof(char*) * args->argc);
	args->argv[args->argc - 1] = str;

	return 0;
}

void freerdp_device_collection_add(rdpSettings* settings, RDPDR_DEVICE* device)
{
	if (settings->DeviceArraySize < (settings->DeviceCount + 1))
	{
		settings->DeviceArraySize *= 2;
		settings->DeviceArray = (RDPDR_DEVICE**)
				realloc(settings->DeviceArray, settings->DeviceArraySize * sizeof(RDPDR_DEVICE*));
	}

	settings->DeviceArray[settings->DeviceCount++] = device;
}

RDPDR_DEVICE* freerdp_device_collection_find(rdpSettings* settings, const char* name)
{
	int index;
	RDPDR_DEVICE* device;

	for (index = 0; index < settings->DeviceCount; index++)
	{
		device = (RDPDR_DEVICE*) settings->DeviceArray[index];

		if (NULL == device->Name)
			continue;

		if (strcmp(device->Name, name) == 0)
			return device;
	}

	return NULL;
}

void freerdp_device_collection_free(rdpSettings* settings)
{
	int index;
	RDPDR_DEVICE* device;

	for (index = 0; index < settings->DeviceCount; index++)
	{
		device = (RDPDR_DEVICE*) settings->DeviceArray[index];

		free(device->Name);

		if (settings->DeviceArray[index]->Type == RDPDR_DTYP_FILESYSTEM)
		{
			free(((RDPDR_DRIVE*) device)->Path);
		}
		else if (settings->DeviceArray[index]->Type == RDPDR_DTYP_PRINT)
		{

		}
		else if (settings->DeviceArray[index]->Type == RDPDR_DTYP_SMARTCARD)
		{
			free(((RDPDR_SMARTCARD*) device)->Path);
		}
		else if (settings->DeviceArray[index]->Type == RDPDR_DTYP_SERIAL)
		{
			free(((RDPDR_SERIAL*) device)->Path);
		}
		else if (settings->DeviceArray[index]->Type == RDPDR_DTYP_PARALLEL)
		{
			free(((RDPDR_PARALLEL*) device)->Path);
		}

		free(device);
	}

	free(settings->DeviceArray);

	settings->DeviceArraySize = 0;
	settings->DeviceArray = NULL;
	settings->DeviceCount = 0;
}

void freerdp_static_channel_collection_add(rdpSettings* settings, ADDIN_ARGV* channel)
{
	if (settings->StaticChannelArraySize < (settings->StaticChannelCount + 1))
	{
		settings->StaticChannelArraySize *= 2;
		settings->StaticChannelArray = (ADDIN_ARGV**)
				realloc(settings->StaticChannelArray, settings->StaticChannelArraySize * sizeof(ADDIN_ARGV*));
	}

	settings->StaticChannelArray[settings->StaticChannelCount++] = channel;
}

ADDIN_ARGV* freerdp_static_channel_collection_find(rdpSettings* settings, const char* name)
{
	int index;
	ADDIN_ARGV* channel;

	for (index = 0; index < settings->StaticChannelCount; index++)
	{
		channel = settings->StaticChannelArray[index];

		if (strcmp(channel->argv[0], name) == 0)
			return channel;
	}

	return NULL;
}

void freerdp_static_channel_collection_free(rdpSettings* settings)
{
	int i, j;

	for (i = 0; i < settings->StaticChannelCount; i++)
	{
		for (j = 0; j < settings->StaticChannelArray[i]->argc; j++)
			free(settings->StaticChannelArray[i]->argv[j]);

		free(settings->StaticChannelArray[i]->argv);
		free(settings->StaticChannelArray[i]);
	}

	free(settings->StaticChannelArray);

	settings->StaticChannelArraySize = 0;
	settings->StaticChannelArray = NULL;
	settings->StaticChannelCount = 0;
}

void freerdp_dynamic_channel_collection_add(rdpSettings* settings, ADDIN_ARGV* channel)
{
	if (settings->DynamicChannelArraySize < (settings->DynamicChannelCount + 1))
	{
		settings->DynamicChannelArraySize *= 2;
		settings->DynamicChannelArray = (ADDIN_ARGV**)
				realloc(settings->DynamicChannelArray, settings->DynamicChannelArraySize * sizeof(ADDIN_ARGV*));
	}

	settings->DynamicChannelArray[settings->DynamicChannelCount++] = channel;
}

ADDIN_ARGV* freerdp_dynamic_channel_collection_find(rdpSettings* settings, const char* name)
{
	int index;
	ADDIN_ARGV* channel;

	for (index = 0; index < settings->DynamicChannelCount; index++)
	{
		channel = settings->DynamicChannelArray[index];

		if (strcmp(channel->argv[0], name) == 0)
			return channel;
	}

	return NULL;
}

void freerdp_dynamic_channel_collection_free(rdpSettings* settings)
{
	int index;

	for (index = 0; index < settings->DynamicChannelCount; index++)
	{
		free(settings->DynamicChannelArray[index]);
	}

	free(settings->DynamicChannelArray);

	settings->DynamicChannelArraySize = 0;
	settings->DynamicChannelArray = NULL;
	settings->DynamicChannelCount = 0;
}

void freerdp_performance_flags_make(rdpSettings* settings)
{
	settings->PerformanceFlags = PERF_FLAG_NONE;

	if (settings->AllowFontSmoothing)
		settings->PerformanceFlags |= PERF_ENABLE_FONT_SMOOTHING;

	if (settings->AllowDesktopComposition)
		settings->PerformanceFlags |= PERF_ENABLE_DESKTOP_COMPOSITION;

	if (settings->DisableWallpaper)
		settings->PerformanceFlags |= PERF_DISABLE_WALLPAPER;

	if (settings->DisableFullWindowDrag)
		settings->PerformanceFlags |= PERF_DISABLE_FULLWINDOWDRAG;

	if (settings->DisableMenuAnims)
		settings->PerformanceFlags |= PERF_DISABLE_MENUANIMATIONS;

	if (settings->DisableThemes)
		settings->PerformanceFlags |= PERF_DISABLE_THEMING;
}

void freerdp_performance_flags_split(rdpSettings* settings)
{
	settings->AllowFontSmoothing = (settings->PerformanceFlags & PERF_ENABLE_FONT_SMOOTHING) ? TRUE : FALSE;

	settings->AllowDesktopComposition = (settings->PerformanceFlags & PERF_ENABLE_DESKTOP_COMPOSITION) ? TRUE : FALSE;

	settings->DisableWallpaper = (settings->PerformanceFlags & PERF_DISABLE_WALLPAPER) ? TRUE : FALSE;

	settings->DisableFullWindowDrag = (settings->PerformanceFlags & PERF_DISABLE_FULLWINDOWDRAG) ? TRUE : FALSE;

	settings->DisableMenuAnims = (settings->PerformanceFlags & PERF_DISABLE_MENUANIMATIONS) ? TRUE : FALSE;

	settings->DisableThemes = (settings->PerformanceFlags & PERF_DISABLE_THEMING) ? TRUE : FALSE;
}

/**
 * Partially Generated Code
 */

BOOL freerdp_get_param_bool(rdpSettings* settings, int id)
{
	switch (id)
	{
		case FreeRDP_ServerMode:
			return settings->ServerMode;
			break;

		case FreeRDP_NetworkAutoDetect:
			return settings->NetworkAutoDetect;
			break;

		case FreeRDP_SupportAsymetricKeys:
			return settings->SupportAsymetricKeys;
			break;

		case FreeRDP_SupportErrorInfoPdu:
			return settings->SupportErrorInfoPdu;
			break;

		case FreeRDP_SupportStatusInfoPdu:
			return settings->SupportStatusInfoPdu;
			break;

		case FreeRDP_SupportMonitorLayoutPdu:
			return settings->SupportMonitorLayoutPdu;
			break;

		case FreeRDP_SupportGraphicsPipeline:
			return settings->SupportGraphicsPipeline;
			break;

		case FreeRDP_SupportDynamicTimeZone:
			return settings->SupportDynamicTimeZone;
			break;

		case FreeRDP_DisableEncryption:
			return settings->DisableEncryption;
			break;

		case FreeRDP_ConsoleSession:
			return settings->ConsoleSession;
			break;

		case FreeRDP_SpanMonitors:
			return settings->SpanMonitors;
			break;

		case FreeRDP_UseMultimon:
			return settings->UseMultimon;
			break;

		case FreeRDP_ForceMultimon:
			return settings->ForceMultimon;
			break;

		case FreeRDP_AutoLogonEnabled:
			return settings->AutoLogonEnabled;
			break;

		case FreeRDP_CompressionEnabled:
			return settings->CompressionEnabled;
			break;

		case FreeRDP_DisableCtrlAltDel:
			return settings->DisableCtrlAltDel;
			break;

		case FreeRDP_EnableWindowsKey:
			return settings->EnableWindowsKey;
			break;

		case FreeRDP_MaximizeShell:
			return settings->MaximizeShell;
			break;

		case FreeRDP_LogonNotify:
			return settings->LogonNotify;
			break;

		case FreeRDP_LogonErrors:
			return settings->LogonErrors;
			break;

		case FreeRDP_MouseAttached:
			return settings->MouseAttached;
			break;

		case FreeRDP_MouseHasWheel:
			return settings->MouseHasWheel;
			break;

		case FreeRDP_RemoteConsoleAudio:
			return settings->RemoteConsoleAudio;
			break;

		case FreeRDP_AudioPlayback:
			return settings->AudioPlayback;
			break;

		case FreeRDP_AudioCapture:
			return settings->AudioCapture;
			break;

		case FreeRDP_VideoDisable:
			return settings->VideoDisable;
			break;

		case FreeRDP_PasswordIsSmartcardPin:
			return settings->PasswordIsSmartcardPin;
			break;

		case FreeRDP_UsingSavedCredentials:
			return settings->UsingSavedCredentials;
			break;

		case FreeRDP_ForceEncryptedCsPdu:
			return settings->ForceEncryptedCsPdu;
			break;

		case FreeRDP_IPv6Enabled:
			return settings->IPv6Enabled;
			break;

		case FreeRDP_AutoReconnectionEnabled:
			return settings->AutoReconnectionEnabled;
			break;

		case FreeRDP_DynamicDaylightTimeDisabled:
			return settings->DynamicDaylightTimeDisabled;
			break;

		case FreeRDP_AllowFontSmoothing:
			return settings->AllowFontSmoothing;
			break;

		case FreeRDP_DisableWallpaper:
			return settings->DisableWallpaper;
			break;

		case FreeRDP_DisableFullWindowDrag:
			return settings->DisableFullWindowDrag;
			break;

		case FreeRDP_DisableMenuAnims:
			return settings->DisableMenuAnims;
			break;

		case FreeRDP_DisableThemes:
			return settings->DisableThemes;
			break;

		case FreeRDP_DisableCursorShadow:
			return settings->DisableCursorShadow;
			break;

		case FreeRDP_DisableCursorBlinking:
			return settings->DisableCursorBlinking;
			break;

		case FreeRDP_AllowDesktopComposition:
			return settings->AllowDesktopComposition;
			break;

		case FreeRDP_TlsSecurity:
			return settings->TlsSecurity;
			break;

		case FreeRDP_NlaSecurity:
			return settings->NlaSecurity;
			break;

		case FreeRDP_RdpSecurity:
			return settings->RdpSecurity;
			break;

		case FreeRDP_ExtSecurity:
			return settings->ExtSecurity;
			break;

		case FreeRDP_Authentication:
			return settings->Authentication;
			break;

		case FreeRDP_NegotiateSecurityLayer:
			return settings->NegotiateSecurityLayer;
			break;

		case FreeRDP_MstscCookieMode:
			return settings->MstscCookieMode;
			break;

		case FreeRDP_SendPreconnectionPdu:
			return settings->SendPreconnectionPdu;
			break;

		case FreeRDP_IgnoreCertificate:
			return settings->IgnoreCertificate;
			break;

		case FreeRDP_Workarea:
			return settings->Workarea;
			break;

		case FreeRDP_Fullscreen:
			return settings->Fullscreen;
			break;

		case FreeRDP_GrabKeyboard:
			return settings->GrabKeyboard;
			break;

		case FreeRDP_Decorations:
			return settings->Decorations;
			break;

		case FreeRDP_SmartSizing:
			return settings->SmartSizing;
			break;

		case FreeRDP_MouseMotion:
			return settings->MouseMotion;
			break;

		case FreeRDP_AsyncInput:
			return settings->AsyncInput;
			break;

		case FreeRDP_AsyncUpdate:
			return settings->AsyncUpdate;
			break;

		case FreeRDP_AsyncChannels:
			return settings->AsyncChannels;
			break;

		case FreeRDP_ToggleFullscreen:
			return settings->ToggleFullscreen;
			break;

		case FreeRDP_SoftwareGdi:
			return settings->SoftwareGdi;
			break;

		case FreeRDP_LocalConnection:
			return settings->LocalConnection;
			break;

		case FreeRDP_AuthenticationOnly:
			return settings->AuthenticationOnly;
			break;

		case FreeRDP_CredentialsFromStdin:
			return settings->CredentialsFromStdin;
			break;

		case FreeRDP_DumpRemoteFx:
			return settings->DumpRemoteFx;
			break;

		case FreeRDP_PlayRemoteFx:
			return settings->PlayRemoteFx;
			break;

		case FreeRDP_GatewayUseSameCredentials:
			return settings->GatewayUseSameCredentials;
			break;

		case FreeRDP_GatewayEnabled:
			return settings->GatewayEnabled;
			break;

		case FreeRDP_RemoteApplicationMode:
			return settings->RemoteApplicationMode;
			break;

		case FreeRDP_DisableRemoteAppCapsCheck:
			return settings->DisableRemoteAppCapsCheck;
			break;

		case FreeRDP_RemoteAppLanguageBarSupported:
			return settings->RemoteAppLanguageBarSupported;
			break;

		case FreeRDP_RefreshRect:
			return settings->RefreshRect;
			break;

		case FreeRDP_SuppressOutput:
			return settings->SuppressOutput;
			break;

		case FreeRDP_FastPathOutput:
			return settings->FastPathOutput;
			break;

		case FreeRDP_SaltedChecksum:
			return settings->SaltedChecksum;
			break;

		case FreeRDP_LongCredentialsSupported:
			return settings->LongCredentialsSupported;
			break;

		case FreeRDP_NoBitmapCompressionHeader:
			return settings->NoBitmapCompressionHeader;
			break;

		case FreeRDP_BitmapCompressionDisabled:
			return settings->BitmapCompressionDisabled;
			break;

		case FreeRDP_DesktopResize:
			return settings->DesktopResize;
			break;

		case FreeRDP_DrawAllowDynamicColorFidelity:
			return settings->DrawAllowDynamicColorFidelity;
			break;

		case FreeRDP_DrawAllowColorSubsampling:
			return settings->DrawAllowColorSubsampling;
			break;

		case FreeRDP_DrawAllowSkipAlpha:
			return settings->DrawAllowSkipAlpha;
			break;

		case FreeRDP_BitmapCacheV3Enabled:
			return settings->BitmapCacheV3Enabled;
			break;

		case FreeRDP_AltSecFrameMarkerSupport:
			return settings->AltSecFrameMarkerSupport;
			break;

		case FreeRDP_BitmapCacheEnabled:
			return settings->BitmapCacheEnabled;
			break;

		case FreeRDP_AllowCacheWaitingList:
			return settings->AllowCacheWaitingList;
			break;

		case FreeRDP_BitmapCachePersistEnabled:
			return settings->BitmapCachePersistEnabled;
			break;

		case FreeRDP_ColorPointerFlag:
			return settings->ColorPointerFlag;
			break;

		case FreeRDP_UnicodeInput:
			return settings->UnicodeInput;
			break;

		case FreeRDP_FastPathInput:
			return settings->FastPathInput;
			break;

		case FreeRDP_MultiTouchInput:
			return settings->MultiTouchInput;
			break;

		case FreeRDP_MultiTouchGestures:
			return settings->MultiTouchGestures;
			break;

		case FreeRDP_SoundBeepsEnabled:
			return settings->SoundBeepsEnabled;
			break;

		case FreeRDP_SurfaceCommandsEnabled:
			return settings->SurfaceCommandsEnabled;
			break;

		case FreeRDP_FrameMarkerCommandEnabled:
			return settings->FrameMarkerCommandEnabled;
			break;

		case FreeRDP_RemoteFxOnly:
			return settings->RemoteFxOnly;
			break;

		case FreeRDP_RemoteFxCodec:
			return settings->RemoteFxCodec;
			break;

		case FreeRDP_RemoteFxImageCodec:
			return settings->RemoteFxImageCodec;
			break;

		case FreeRDP_NSCodec:
			return settings->NSCodec;
			break;

		case FreeRDP_FrameAcknowledge:
			return settings->FrameAcknowledge;
			break;

		case FreeRDP_JpegCodec:
			return settings->JpegCodec;
			break;

		case FreeRDP_DrawNineGridEnabled:
			return settings->DrawNineGridEnabled;
			break;

		case FreeRDP_DrawGdiPlusEnabled:
			return settings->DrawGdiPlusEnabled;
			break;

		case FreeRDP_DrawGdiPlusCacheEnabled:
			return settings->DrawGdiPlusCacheEnabled;
			break;

		case FreeRDP_DeviceRedirection:
			return settings->DeviceRedirection;
			break;

		case FreeRDP_RedirectDrives:
			return settings->RedirectDrives;
			break;

		case FreeRDP_RedirectHomeDrive:
			return settings->RedirectHomeDrive;
			break;

		case FreeRDP_RedirectSmartCards:
			return settings->RedirectSmartCards;
			break;

		case FreeRDP_RedirectPrinters:
			return settings->RedirectPrinters;
			break;

		case FreeRDP_RedirectSerialPorts:
			return settings->RedirectSerialPorts;
			break;

		case FreeRDP_RedirectParallelPorts:
			return settings->RedirectParallelPorts;
			break;

		case FreeRDP_RedirectClipboard:
			return settings->RedirectClipboard;
			break;

		default:
			return -1;
			break;
	}

	return -1;
}

int freerdp_set_param_bool(rdpSettings* settings, int id, BOOL param)
{
	ParamChangeEventArgs e;
	rdpContext* context = ((freerdp*) settings->instance)->context;

	switch (id)
	{
		case FreeRDP_ServerMode:
			settings->ServerMode = param;
			break;

		case FreeRDP_NetworkAutoDetect:
			settings->NetworkAutoDetect = param;
			break;

		case FreeRDP_SupportAsymetricKeys:
			settings->SupportAsymetricKeys = param;
			break;

		case FreeRDP_SupportErrorInfoPdu:
			settings->SupportErrorInfoPdu = param;
			break;

		case FreeRDP_SupportStatusInfoPdu:
			settings->SupportStatusInfoPdu = param;
			break;

		case FreeRDP_SupportMonitorLayoutPdu:
			settings->SupportMonitorLayoutPdu = param;
			break;

		case FreeRDP_SupportGraphicsPipeline:
			settings->SupportGraphicsPipeline = param;
			break;

		case FreeRDP_SupportDynamicTimeZone:
			settings->SupportDynamicTimeZone = param;
			break;

		case FreeRDP_DisableEncryption:
			settings->DisableEncryption = param;
			break;

		case FreeRDP_ConsoleSession:
			settings->ConsoleSession = param;
			break;

		case FreeRDP_SpanMonitors:
			settings->SpanMonitors = param;
			break;

		case FreeRDP_UseMultimon:
			settings->UseMultimon = param;
			break;

		case FreeRDP_ForceMultimon:
			settings->ForceMultimon = param;
			break;

		case FreeRDP_AutoLogonEnabled:
			settings->AutoLogonEnabled = param;
			break;

		case FreeRDP_CompressionEnabled:
			settings->CompressionEnabled = param;
			break;

		case FreeRDP_DisableCtrlAltDel:
			settings->DisableCtrlAltDel = param;
			break;

		case FreeRDP_EnableWindowsKey:
			settings->EnableWindowsKey = param;
			break;

		case FreeRDP_MaximizeShell:
			settings->MaximizeShell = param;
			break;

		case FreeRDP_LogonNotify:
			settings->LogonNotify = param;
			break;

		case FreeRDP_LogonErrors:
			settings->LogonErrors = param;
			break;

		case FreeRDP_MouseAttached:
			settings->MouseAttached = param;
			break;

		case FreeRDP_MouseHasWheel:
			settings->MouseHasWheel = param;
			break;

		case FreeRDP_RemoteConsoleAudio:
			settings->RemoteConsoleAudio = param;
			break;

		case FreeRDP_AudioPlayback:
			settings->AudioPlayback = param;
			break;

		case FreeRDP_AudioCapture:
			settings->AudioCapture = param;
			break;

		case FreeRDP_VideoDisable:
			settings->VideoDisable = param;
			break;

		case FreeRDP_PasswordIsSmartcardPin:
			settings->PasswordIsSmartcardPin = param;
			break;

		case FreeRDP_UsingSavedCredentials:
			settings->UsingSavedCredentials = param;
			break;

		case FreeRDP_ForceEncryptedCsPdu:
			settings->ForceEncryptedCsPdu = param;
			break;

		case FreeRDP_IPv6Enabled:
			settings->IPv6Enabled = param;
			break;

		case FreeRDP_AutoReconnectionEnabled:
			settings->AutoReconnectionEnabled = param;
			break;

		case FreeRDP_DynamicDaylightTimeDisabled:
			settings->DynamicDaylightTimeDisabled = param;
			break;

		case FreeRDP_AllowFontSmoothing:
			settings->AllowFontSmoothing = param;
			break;

		case FreeRDP_DisableWallpaper:
			settings->DisableWallpaper = param;
			break;

		case FreeRDP_DisableFullWindowDrag:
			settings->DisableFullWindowDrag = param;
			break;

		case FreeRDP_DisableMenuAnims:
			settings->DisableMenuAnims = param;
			break;

		case FreeRDP_DisableThemes:
			settings->DisableThemes = param;
			break;

		case FreeRDP_DisableCursorShadow:
			settings->DisableCursorShadow = param;
			break;

		case FreeRDP_DisableCursorBlinking:
			settings->DisableCursorBlinking = param;
			break;

		case FreeRDP_AllowDesktopComposition:
			settings->AllowDesktopComposition = param;
			break;

		case FreeRDP_TlsSecurity:
			settings->TlsSecurity = param;
			break;

		case FreeRDP_NlaSecurity:
			settings->NlaSecurity = param;
			break;

		case FreeRDP_RdpSecurity:
			settings->RdpSecurity = param;
			break;

		case FreeRDP_ExtSecurity:
			settings->ExtSecurity = param;
			break;

		case FreeRDP_Authentication:
			settings->Authentication = param;
			break;

		case FreeRDP_NegotiateSecurityLayer:
			settings->NegotiateSecurityLayer = param;
			break;

		case FreeRDP_MstscCookieMode:
			settings->MstscCookieMode = param;
			break;

		case FreeRDP_SendPreconnectionPdu:
			settings->SendPreconnectionPdu = param;
			break;

		case FreeRDP_IgnoreCertificate:
			settings->IgnoreCertificate = param;
			break;

		case FreeRDP_Workarea:
			settings->Workarea = param;
			break;

		case FreeRDP_Fullscreen:
			settings->Fullscreen = param;
			break;

		case FreeRDP_GrabKeyboard:
			settings->GrabKeyboard = param;
			break;

		case FreeRDP_Decorations:
			settings->Decorations = param;
			break;

		case FreeRDP_SmartSizing:
			settings->SmartSizing = param;
			break;		

		case FreeRDP_MouseMotion:
			settings->MouseMotion = param;
			break;

		case FreeRDP_AsyncInput:
			settings->AsyncInput = param;
			break;

		case FreeRDP_AsyncUpdate:
			settings->AsyncUpdate = param;
			break;

		case FreeRDP_AsyncChannels:
			settings->AsyncChannels = param;
			break;

		case FreeRDP_ToggleFullscreen:
			settings->ToggleFullscreen = param;
			break;

		case FreeRDP_SoftwareGdi:
			settings->SoftwareGdi = param;
			break;

		case FreeRDP_LocalConnection:
			settings->LocalConnection = param;
			break;

		case FreeRDP_AuthenticationOnly:
			settings->AuthenticationOnly = param;
			break;

		case FreeRDP_CredentialsFromStdin:
			settings->CredentialsFromStdin = param;
			break;

		case FreeRDP_DumpRemoteFx:
			settings->DumpRemoteFx = param;
			break;

		case FreeRDP_PlayRemoteFx:
			settings->PlayRemoteFx = param;
			break;

		case FreeRDP_GatewayUseSameCredentials:
			settings->GatewayUseSameCredentials = param;
			break;

		case FreeRDP_GatewayEnabled:
			settings->GatewayEnabled = param;
			break;

		case FreeRDP_RemoteApplicationMode:
			settings->RemoteApplicationMode = param;
			break;

		case FreeRDP_DisableRemoteAppCapsCheck:
			settings->DisableRemoteAppCapsCheck = param;
			break;

		case FreeRDP_RemoteAppLanguageBarSupported:
			settings->RemoteAppLanguageBarSupported = param;
			break;

		case FreeRDP_RefreshRect:
			settings->RefreshRect = param;
			break;

		case FreeRDP_SuppressOutput:
			settings->SuppressOutput = param;
			break;

		case FreeRDP_FastPathOutput:
			settings->FastPathOutput = param;
			break;

		case FreeRDP_SaltedChecksum:
			settings->SaltedChecksum = param;
			break;

		case FreeRDP_LongCredentialsSupported:
			settings->LongCredentialsSupported = param;
			break;

		case FreeRDP_NoBitmapCompressionHeader:
			settings->NoBitmapCompressionHeader = param;
			break;

		case FreeRDP_BitmapCompressionDisabled:
			settings->BitmapCompressionDisabled = param;
			break;

		case FreeRDP_DesktopResize:
			settings->DesktopResize = param;
			break;

		case FreeRDP_DrawAllowDynamicColorFidelity:
			settings->DrawAllowDynamicColorFidelity = param;
			break;

		case FreeRDP_DrawAllowColorSubsampling:
			settings->DrawAllowColorSubsampling = param;
			break;

		case FreeRDP_DrawAllowSkipAlpha:
			settings->DrawAllowSkipAlpha = param;
			break;

		case FreeRDP_BitmapCacheV3Enabled:
			settings->BitmapCacheV3Enabled = param;
			break;

		case FreeRDP_AltSecFrameMarkerSupport:
			settings->AltSecFrameMarkerSupport = param;
			break;

		case FreeRDP_BitmapCacheEnabled:
			settings->BitmapCacheEnabled = param;
			break;

		case FreeRDP_AllowCacheWaitingList:
			settings->AllowCacheWaitingList = param;
			break;

		case FreeRDP_BitmapCachePersistEnabled:
			settings->BitmapCachePersistEnabled = param;
			break;

		case FreeRDP_ColorPointerFlag:
			settings->ColorPointerFlag = param;
			break;

		case FreeRDP_UnicodeInput:
			settings->UnicodeInput = param;
			break;

		case FreeRDP_FastPathInput:
			settings->FastPathInput = param;
			break;

		case FreeRDP_MultiTouchInput:
			settings->MultiTouchInput = param;
			break;

		case FreeRDP_MultiTouchGestures:
			settings->MultiTouchGestures = param;
			break;

		case FreeRDP_SoundBeepsEnabled:
			settings->SoundBeepsEnabled = param;
			break;

		case FreeRDP_SurfaceCommandsEnabled:
			settings->SurfaceCommandsEnabled = param;
			break;

		case FreeRDP_FrameMarkerCommandEnabled:
			settings->FrameMarkerCommandEnabled = param;
			break;

		case FreeRDP_RemoteFxOnly:
			settings->RemoteFxOnly = param;
			break;

		case FreeRDP_RemoteFxCodec:
			settings->RemoteFxCodec = param;
			break;

		case FreeRDP_RemoteFxImageCodec:
			settings->RemoteFxImageCodec = param;
			break;

		case FreeRDP_NSCodec:
			settings->NSCodec = param;
			break;

		case FreeRDP_FrameAcknowledge:
			settings->FrameAcknowledge = param;
			break;

		case FreeRDP_JpegCodec:
			settings->JpegCodec = param;
			break;

		case FreeRDP_DrawNineGridEnabled:
			settings->DrawNineGridEnabled = param;
			break;

		case FreeRDP_DrawGdiPlusEnabled:
			settings->DrawGdiPlusEnabled = param;
			break;

		case FreeRDP_DrawGdiPlusCacheEnabled:
			settings->DrawGdiPlusCacheEnabled = param;
			break;

		case FreeRDP_DeviceRedirection:
			settings->DeviceRedirection = param;
			break;

		case FreeRDP_RedirectDrives:
			settings->RedirectDrives = param;
			break;

		case FreeRDP_RedirectHomeDrive:
			settings->RedirectHomeDrive = param;
			break;

		case FreeRDP_RedirectSmartCards:
			settings->RedirectSmartCards = param;
			break;

		case FreeRDP_RedirectPrinters:
			settings->RedirectPrinters = param;
			break;

		case FreeRDP_RedirectSerialPorts:
			settings->RedirectSerialPorts = param;
			break;

		case FreeRDP_RedirectParallelPorts:
			settings->RedirectParallelPorts = param;
			break;

		case FreeRDP_RedirectClipboard:
			settings->RedirectClipboard = param;
			break;

		default:
			return -1;
			break;
	}

	// Mark field as modified
	settings->settings_modified[id] = 1;

	EventArgsInit(&e, "freerdp");
	e.id = id;
	PubSub_OnParamChange(context->pubSub, context, &e);

	return -1;
}

int freerdp_get_param_int(rdpSettings* settings, int id)
{
	switch (id)
	{
		case FreeRDP_XPan:
			return settings->XPan;
			break;

		case FreeRDP_YPan:
			return settings->YPan;
			break;

		default:
			return 0;
			break;
	}

	return 0;
}

int freerdp_set_param_int(rdpSettings* settings, int id, int param)
{
	ParamChangeEventArgs e;
	rdpContext* context = ((freerdp*) settings->instance)->context;

	switch (id)
	{
		case FreeRDP_XPan:
			settings->XPan = param;
			break;

		case FreeRDP_YPan:
			settings->YPan = param;
			break;

		default:
			return -1;
			break;
	}

	settings->settings_modified[id] = 1;

	EventArgsInit(&e, "freerdp");
	e.id = id;
	PubSub_OnParamChange(context->pubSub, context, &e);

	return 0;
}

UINT32 freerdp_get_param_uint32(rdpSettings* settings, int id)
{
	switch (id)
	{
		case FreeRDP_ShareId:
			return settings->ShareId;
			break;

		case FreeRDP_PduSource:
			return settings->PduSource;
			break;

		case FreeRDP_ServerPort:
			return settings->ServerPort;
			break;

		case FreeRDP_RdpVersion:
			return settings->RdpVersion;
			break;

		case FreeRDP_DesktopWidth:
			return settings->DesktopWidth;
			break;

		case FreeRDP_DesktopHeight:
			return settings->DesktopHeight;
			break;

		case FreeRDP_ColorDepth:
			return settings->ColorDepth;
			break;

		case FreeRDP_ConnectionType:
			return settings->ConnectionType;
			break;

		case FreeRDP_ClientBuild:
			return settings->ClientBuild;
			break;

		case FreeRDP_EarlyCapabilityFlags:
			return settings->EarlyCapabilityFlags;
			break;

		case FreeRDP_EncryptionMethods:
			return settings->EncryptionMethods;
			break;

		case FreeRDP_ExtEncryptionMethods:
			return settings->ExtEncryptionMethods;
			break;

		case FreeRDP_EncryptionLevel:
			return settings->EncryptionLevel;
			break;

		case FreeRDP_ChannelCount:
			return settings->ChannelCount;
			break;

		case FreeRDP_ChannelDefArraySize:
			return settings->ChannelDefArraySize;
			break;

		case FreeRDP_ClusterInfoFlags:
			return settings->ClusterInfoFlags;
			break;

		case FreeRDP_RedirectedSessionId:
			return settings->RedirectedSessionId;
			break;

		case FreeRDP_MonitorDefArraySize:
			return settings->MonitorDefArraySize;
			break;

		case FreeRDP_DesktopPosX:
			return settings->DesktopPosX;
			break;

		case FreeRDP_DesktopPosY:
			return settings->DesktopPosY;
			break;

		case FreeRDP_MultitransportFlags:
			return settings->MultitransportFlags;
			break;

		case FreeRDP_AutoReconnectMaxRetries:
			return settings->AutoReconnectMaxRetries;
			break;

		case FreeRDP_PerformanceFlags:
			return settings->PerformanceFlags;
			break;

		case FreeRDP_RequestedProtocols:
			return settings->RequestedProtocols;
			break;

		case FreeRDP_SelectedProtocol:
			return settings->SelectedProtocol;
			break;

		case FreeRDP_NegotiationFlags:
			return settings->NegotiationFlags;
			break;

		case FreeRDP_CookieMaxLength:
			return settings->CookieMaxLength;
			break;

		case FreeRDP_PreconnectionId:
			return settings->PreconnectionId;
			break;

		case FreeRDP_RedirectionFlags:
			return settings->RedirectionFlags;
			break;

		case FreeRDP_LoadBalanceInfoLength:
			return settings->LoadBalanceInfoLength;
			break;

		case FreeRDP_RedirectionUsernameLength:
			return settings->RedirectionUsernameLength;
			break;

		case FreeRDP_RedirectionDomainLength:
			return settings->RedirectionDomainLength;
			break;

		case FreeRDP_RedirectionPasswordLength:
			return settings->RedirectionPasswordLength;
			break;

		case FreeRDP_RedirectionTargetFQDNLength:
			return settings->RedirectionTargetFQDNLength;
			break;

		case FreeRDP_RedirectionTargetNetBiosNameLength:
			return settings->RedirectionTargetNetBiosNameLength;
			break;

		case FreeRDP_RedirectionTsvUrlLength:
			return settings->RedirectionTsvUrlLength;
			break;

		case FreeRDP_TargetNetAddressCount:
			return settings->TargetNetAddressCount;
			break;

		case FreeRDP_PercentScreen:
			return settings->PercentScreen;
			break;

		case FreeRDP_GatewayUsageMethod:
			return settings->GatewayUsageMethod;
			break;

		case FreeRDP_GatewayPort:
			return settings->GatewayPort;
			break;

		case FreeRDP_GatewayCredentialsSource:
			return settings->GatewayCredentialsSource;
			break;

		case FreeRDP_RemoteAppNumIconCaches:
			return settings->RemoteAppNumIconCaches;
			break;

		case FreeRDP_RemoteAppNumIconCacheEntries:
			return settings->RemoteAppNumIconCacheEntries;
			break;

		case FreeRDP_ReceivedCapabilitiesSize:
			return settings->ReceivedCapabilitiesSize;
			break;

		case FreeRDP_OsMajorType:
			return settings->OsMajorType;
			break;

		case FreeRDP_OsMinorType:
			return settings->OsMinorType;
			break;

		case FreeRDP_BitmapCacheVersion:
			return settings->BitmapCacheVersion;
			break;

		case FreeRDP_BitmapCacheV2NumCells:
			return settings->BitmapCacheV2NumCells;
			break;

		case FreeRDP_PointerCacheSize:
			return settings->PointerCacheSize;
			break;

		case FreeRDP_KeyboardLayout:
			return settings->KeyboardLayout;
			break;

		case FreeRDP_KeyboardType:
			return settings->KeyboardType;
			break;

		case FreeRDP_KeyboardSubType:
			return settings->KeyboardSubType;
			break;

		case FreeRDP_KeyboardFunctionKey:
			return settings->KeyboardFunctionKey;
			break;

		case FreeRDP_BrushSupportLevel:
			return settings->BrushSupportLevel;
			break;

		case FreeRDP_GlyphSupportLevel:
			return settings->GlyphSupportLevel;
			break;

		case FreeRDP_OffscreenSupportLevel:
			return settings->OffscreenSupportLevel;
			break;

		case FreeRDP_OffscreenCacheSize:
			return settings->OffscreenCacheSize;
			break;

		case FreeRDP_OffscreenCacheEntries:
			return settings->OffscreenCacheEntries;
			break;

		case FreeRDP_VirtualChannelCompressionFlags:
			return settings->VirtualChannelCompressionFlags;
			break;

		case FreeRDP_VirtualChannelChunkSize:
			return settings->VirtualChannelChunkSize;
			break;

		case FreeRDP_MultifragMaxRequestSize:
			return settings->MultifragMaxRequestSize;
			break;

		case FreeRDP_LargePointerFlag:
			return settings->LargePointerFlag;
			break;

		case FreeRDP_CompDeskSupportLevel:
			return settings->CompDeskSupportLevel;
			break;

		case FreeRDP_RemoteFxCodecId:
			return settings->RemoteFxCodecId;
			break;

		case FreeRDP_RemoteFxCodecMode:
			return settings->RemoteFxCodecMode;
			break;

		case FreeRDP_NSCodecId:
			return settings->NSCodecId;
			break;

		case FreeRDP_JpegCodecId:
			return settings->JpegCodecId;
			break;

		case FreeRDP_JpegQuality:
			return settings->JpegQuality;
			break;

		case FreeRDP_BitmapCacheV3CodecId:
			return settings->BitmapCacheV3CodecId;
			break;

		case FreeRDP_DrawNineGridCacheSize:
			return settings->DrawNineGridCacheSize;
			break;

		case FreeRDP_DrawNineGridCacheEntries:
			return settings->DrawNineGridCacheEntries;
			break;

		case FreeRDP_DeviceCount:
			return settings->DeviceCount;
			break;

		case FreeRDP_DeviceArraySize:
			return settings->DeviceArraySize;
			break;

		case FreeRDP_StaticChannelCount:
			return settings->StaticChannelCount;
			break;

		case FreeRDP_StaticChannelArraySize:
			return settings->StaticChannelArraySize;
			break;

		case FreeRDP_DynamicChannelCount:
			return settings->DynamicChannelCount;
			break;

		case FreeRDP_DynamicChannelArraySize:
			return settings->DynamicChannelArraySize;
			break;

		default:
			return 0;
			break;
	}

	return 0;
}

int freerdp_set_param_uint32(rdpSettings* settings, int id, UINT32 param)
{
	ParamChangeEventArgs e;
	rdpContext* context = ((freerdp*) settings->instance)->context;

	switch (id)
	{
		case FreeRDP_ShareId:
			settings->ShareId = param;
			break;

		case FreeRDP_PduSource:
			settings->PduSource = param;
			break;

		case FreeRDP_ServerPort:
			settings->ServerPort = param;
			break;

		case FreeRDP_RdpVersion:
			settings->RdpVersion = param;
			break;

		case FreeRDP_DesktopWidth:
			settings->DesktopWidth = param;
			break;

		case FreeRDP_DesktopHeight:
			settings->DesktopHeight = param;
			break;

		case FreeRDP_ColorDepth:
			settings->ColorDepth = param;
			break;

		case FreeRDP_ConnectionType:
			settings->ConnectionType = param;
			break;

		case FreeRDP_ClientBuild:
			settings->ClientBuild = param;
			break;

		case FreeRDP_EarlyCapabilityFlags:
			settings->EarlyCapabilityFlags = param;
			break;

		case FreeRDP_EncryptionMethods:
			settings->EncryptionMethods = param;
			break;

		case FreeRDP_ExtEncryptionMethods:
			settings->ExtEncryptionMethods = param;
			break;

		case FreeRDP_EncryptionLevel:
			settings->EncryptionLevel = param;
			break;

		case FreeRDP_ChannelCount:
			settings->ChannelCount = param;
			break;

		case FreeRDP_ChannelDefArraySize:
			settings->ChannelDefArraySize = param;
			break;

		case FreeRDP_ClusterInfoFlags:
			settings->ClusterInfoFlags = param;
			break;

		case FreeRDP_RedirectedSessionId:
			settings->RedirectedSessionId = param;
			break;

		case FreeRDP_MonitorDefArraySize:
			settings->MonitorDefArraySize = param;
			break;

		case FreeRDP_DesktopPosX:
			settings->DesktopPosX = param;
			break;

		case FreeRDP_DesktopPosY:
			settings->DesktopPosY = param;
			break;

		case FreeRDP_MultitransportFlags:
			settings->MultitransportFlags = param;
			break;

		case FreeRDP_AutoReconnectMaxRetries:
			settings->AutoReconnectMaxRetries = param;
			break;

		case FreeRDP_PerformanceFlags:
			settings->PerformanceFlags = param;
			break;

		case FreeRDP_RequestedProtocols:
			settings->RequestedProtocols = param;
			break;

		case FreeRDP_SelectedProtocol:
			settings->SelectedProtocol = param;
			break;

		case FreeRDP_NegotiationFlags:
			settings->NegotiationFlags = param;
			break;

		case FreeRDP_CookieMaxLength:
			settings->CookieMaxLength = param;
			break;

		case FreeRDP_PreconnectionId:
			settings->PreconnectionId = param;
			break;

		case FreeRDP_RedirectionFlags:
			settings->RedirectionFlags = param;
			break;

		case FreeRDP_LoadBalanceInfoLength:
			settings->LoadBalanceInfoLength = param;
			break;

		case FreeRDP_RedirectionUsernameLength:
			settings->RedirectionUsernameLength = param;
			break;

		case FreeRDP_RedirectionDomainLength:
			settings->RedirectionDomainLength = param;
			break;

		case FreeRDP_RedirectionPasswordLength:
			settings->RedirectionPasswordLength = param;
			break;

		case FreeRDP_RedirectionTargetFQDNLength:
			settings->RedirectionTargetFQDNLength = param;
			break;

		case FreeRDP_RedirectionTargetNetBiosNameLength:
			settings->RedirectionTargetNetBiosNameLength = param;
			break;

		case FreeRDP_RedirectionTsvUrlLength:
			settings->RedirectionTsvUrlLength = param;
			break;

		case FreeRDP_TargetNetAddressCount:
			settings->TargetNetAddressCount = param;
			break;

		case FreeRDP_PercentScreen:
			settings->PercentScreen = param;
			break;

		case FreeRDP_GatewayUsageMethod:
			settings->GatewayUsageMethod = param;
			break;

		case FreeRDP_GatewayPort:
			settings->GatewayPort = param;
			break;

		case FreeRDP_GatewayCredentialsSource:
			settings->GatewayCredentialsSource = param;
			break;

		case FreeRDP_RemoteAppNumIconCaches:
			settings->RemoteAppNumIconCaches = param;
			break;

		case FreeRDP_RemoteAppNumIconCacheEntries:
			settings->RemoteAppNumIconCacheEntries = param;
			break;

		case FreeRDP_ReceivedCapabilitiesSize:
			settings->ReceivedCapabilitiesSize = param;
			break;

		case FreeRDP_OsMajorType:
			settings->OsMajorType = param;
			break;

		case FreeRDP_OsMinorType:
			settings->OsMinorType = param;
			break;

		case FreeRDP_BitmapCacheVersion:
			settings->BitmapCacheVersion = param;
			break;

		case FreeRDP_BitmapCacheV2NumCells:
			settings->BitmapCacheV2NumCells = param;
			break;

		case FreeRDP_PointerCacheSize:
			settings->PointerCacheSize = param;
			break;

		case FreeRDP_KeyboardLayout:
			settings->KeyboardLayout = param;
			break;

		case FreeRDP_KeyboardType:
			settings->KeyboardType = param;
			break;

		case FreeRDP_KeyboardSubType:
			settings->KeyboardSubType = param;
			break;

		case FreeRDP_KeyboardFunctionKey:
			settings->KeyboardFunctionKey = param;
			break;

		case FreeRDP_BrushSupportLevel:
			settings->BrushSupportLevel = param;
			break;

		case FreeRDP_GlyphSupportLevel:
			settings->GlyphSupportLevel = param;
			break;

		case FreeRDP_OffscreenSupportLevel:
			settings->OffscreenSupportLevel = param;
			break;

		case FreeRDP_OffscreenCacheSize:
			settings->OffscreenCacheSize = param;
			break;

		case FreeRDP_OffscreenCacheEntries:
			settings->OffscreenCacheEntries = param;
			break;

		case FreeRDP_VirtualChannelCompressionFlags:
			settings->VirtualChannelCompressionFlags = param;
			break;

		case FreeRDP_VirtualChannelChunkSize:
			settings->VirtualChannelChunkSize = param;
			break;

		case FreeRDP_MultifragMaxRequestSize:
			settings->MultifragMaxRequestSize = param;
			break;

		case FreeRDP_LargePointerFlag:
			settings->LargePointerFlag = param;
			break;

		case FreeRDP_CompDeskSupportLevel:
			settings->CompDeskSupportLevel = param;
			break;

		case FreeRDP_RemoteFxCodecId:
			settings->RemoteFxCodecId = param;
			break;

		case FreeRDP_RemoteFxCodecMode:
			settings->RemoteFxCodecMode = param;
			break;

		case FreeRDP_NSCodecId:
			settings->NSCodecId = param;
			break;

		case FreeRDP_JpegCodecId:
			settings->JpegCodecId = param;
			break;

		case FreeRDP_JpegQuality:
			settings->JpegQuality = param;
			break;

		case FreeRDP_BitmapCacheV3CodecId:
			settings->BitmapCacheV3CodecId = param;
			break;

		case FreeRDP_DrawNineGridCacheSize:
			settings->DrawNineGridCacheSize = param;
			break;

		case FreeRDP_DrawNineGridCacheEntries:
			settings->DrawNineGridCacheEntries = param;
			break;

		case FreeRDP_DeviceCount:
			settings->DeviceCount = param;
			break;

		case FreeRDP_DeviceArraySize:
			settings->DeviceArraySize = param;
			break;

		case FreeRDP_StaticChannelCount:
			settings->StaticChannelCount = param;
			break;

		case FreeRDP_StaticChannelArraySize:
			settings->StaticChannelArraySize = param;
			break;

		case FreeRDP_DynamicChannelCount:
			settings->DynamicChannelCount = param;
			break;

		case FreeRDP_DynamicChannelArraySize:
			settings->DynamicChannelArraySize = param;
			break;

		default:
			return -1;
			break;
	}

	// Mark field as modified
	settings->settings_modified[id] = 1;

	EventArgsInit(&e, "freerdp");
	e.id = id;
	PubSub_OnParamChange(context->pubSub, context, &e);
	
	return 0;
}

UINT64 freerdp_get_param_uint64(rdpSettings* settings, int id)
{
	switch (id)
	{
		case FreeRDP_ParentWindowId:
			return settings->ParentWindowId;
			break;

		default:
			return -1;
			break;
	}

	return 0;
}

int freerdp_set_param_uint64(rdpSettings* settings, int id, UINT64 param)
{
	ParamChangeEventArgs e;
	rdpContext* context = ((freerdp*) settings->instance)->context;

	switch (id)
	{
		case FreeRDP_ParentWindowId:
			settings->ParentWindowId = param;
			break;

		default:
			return -1;
			break;
	}

	// Mark field as modified
	settings->settings_modified[id] = 1;

	EventArgsInit(&e, "freerdp");
	e.id = id;
	PubSub_OnParamChange(context->pubSub, context, &e);
	
	return 0;
}

char* freerdp_get_param_string(rdpSettings* settings, int id)
{
	switch (id)
	{
		case FreeRDP_ServerHostname:
			return settings->ServerHostname;
			break;

		case FreeRDP_Username:
			return settings->Username;
			break;

		case FreeRDP_Password:
			return settings->Password;
			break;

		case FreeRDP_Domain:
			return settings->Domain;
			break;

		case FreeRDP_ClientHostname:
			return settings->ClientHostname;
			break;

		case FreeRDP_ClientProductId:
			return settings->ClientProductId;
			break;

		case FreeRDP_AlternateShell:
			return settings->AlternateShell;
			break;

		case FreeRDP_ShellWorkingDirectory:
			return settings->ShellWorkingDirectory;
			break;

		case FreeRDP_ClientAddress:
			return settings->ClientAddress;
			break;

		case FreeRDP_ClientDir:
			return settings->ClientDir;
			break;

		case FreeRDP_DynamicDSTTimeZoneKeyName:
			return settings->DynamicDSTTimeZoneKeyName;
			break;

		case FreeRDP_PreconnectionBlob:
			return settings->PreconnectionBlob;
			break;

		case FreeRDP_KerberosKdc:
			return settings->KerberosKdc;
			break;

		case FreeRDP_KerberosRealm:
			return settings->KerberosRealm;
			break;

		case FreeRDP_CertificateName:
			return settings->CertificateName;
			break;

		case FreeRDP_CertificateFile:
			return settings->CertificateFile;
			break;

		case FreeRDP_PrivateKeyFile:
			return settings->PrivateKeyFile;
			break;

		case FreeRDP_RdpKeyFile:
			return settings->RdpKeyFile;
			break;

		case FreeRDP_WindowTitle:
			return settings->WindowTitle;
			break;

		case FreeRDP_ComputerName:
			return settings->ComputerName;
			break;

		case FreeRDP_ConnectionFile:
			return settings->ConnectionFile;
			break;

		case FreeRDP_HomePath:
			return settings->HomePath;
			break;

		case FreeRDP_ConfigPath:
			return settings->ConfigPath;
			break;

		case FreeRDP_CurrentPath:
			return settings->CurrentPath;
			break;

		case FreeRDP_DumpRemoteFxFile:
			return settings->DumpRemoteFxFile;
			break;

		case FreeRDP_PlayRemoteFxFile:
			return settings->PlayRemoteFxFile;
			break;

		case FreeRDP_GatewayHostname:
			return settings->GatewayHostname;
			break;

		case FreeRDP_GatewayUsername:
			return settings->GatewayUsername;
			break;

		case FreeRDP_GatewayPassword:
			return settings->GatewayPassword;
			break;

		case FreeRDP_GatewayDomain:
			return settings->GatewayDomain;
			break;

		case FreeRDP_RemoteApplicationName:
			return settings->RemoteApplicationName;
			break;

		case FreeRDP_RemoteApplicationIcon:
			return settings->RemoteApplicationIcon;
			break;

		case FreeRDP_RemoteApplicationProgram:
			return settings->RemoteApplicationProgram;
			break;

		case FreeRDP_RemoteApplicationFile:
			return settings->RemoteApplicationFile;
			break;

		case FreeRDP_RemoteApplicationGuid:
			return settings->RemoteApplicationGuid;
			break;

		case FreeRDP_RemoteApplicationCmdLine:
			return settings->RemoteApplicationCmdLine;
			break;

		case FreeRDP_ImeFileName:
			return settings->ImeFileName;
			break;

		case FreeRDP_DrivesToRedirect:
			return settings->DrivesToRedirect;
			break;

		default:
			return NULL;
			break;
	}

	return NULL;
}

int freerdp_set_param_string(rdpSettings* settings, int id, char* param)
{
	ParamChangeEventArgs e;
	rdpContext* context = ((freerdp*) settings->instance)->context;

	switch (id)
	{
		case FreeRDP_ServerHostname:
			settings->ServerHostname = _strdup(param);
			break;

		case FreeRDP_Username:
			settings->Username = _strdup(param);
			break;

		case FreeRDP_Password:
			settings->Password = _strdup(param);
			break;

		case FreeRDP_Domain:
			settings->Domain = _strdup(param);
			break;

		case FreeRDP_ClientHostname:
			settings->ClientHostname = _strdup(param);
			break;

		case FreeRDP_ClientProductId:
			settings->ClientProductId = _strdup(param);
			break;

		case FreeRDP_AlternateShell:
			settings->AlternateShell = _strdup(param);
			break;

		case FreeRDP_ShellWorkingDirectory:
			settings->ShellWorkingDirectory = _strdup(param);
			break;

		case FreeRDP_ClientAddress:
			settings->ClientAddress = _strdup(param);
			break;

		case FreeRDP_ClientDir:
			settings->ClientDir = _strdup(param);
			break;

		case FreeRDP_DynamicDSTTimeZoneKeyName:
			settings->DynamicDSTTimeZoneKeyName = _strdup(param);
			break;

		case FreeRDP_PreconnectionBlob:
			settings->PreconnectionBlob = _strdup(param);
			break;

		case FreeRDP_KerberosKdc:
			settings->KerberosKdc = _strdup(param);
			break;

		case FreeRDP_KerberosRealm:
			settings->KerberosRealm = _strdup(param);
			break;

		case FreeRDP_CertificateName:
			settings->CertificateName = _strdup(param);
			break;

		case FreeRDP_CertificateFile:
			settings->CertificateFile = _strdup(param);
			break;

		case FreeRDP_PrivateKeyFile:
			settings->PrivateKeyFile = _strdup(param);
			break;

		case FreeRDP_RdpKeyFile:
			settings->RdpKeyFile = _strdup(param);
			break;

		case FreeRDP_WindowTitle:
			settings->WindowTitle = _strdup(param);
			break;

		case FreeRDP_ComputerName:
			settings->ComputerName = _strdup(param);
			break;

		case FreeRDP_ConnectionFile:
			settings->ConnectionFile = _strdup(param);
			break;

		case FreeRDP_HomePath:
			settings->HomePath = _strdup(param);
			break;

		case FreeRDP_ConfigPath:
			settings->ConfigPath = _strdup(param);
			break;

		case FreeRDP_CurrentPath:
			settings->CurrentPath = _strdup(param);
			break;

		case FreeRDP_DumpRemoteFxFile:
			settings->DumpRemoteFxFile = _strdup(param);
			break;

		case FreeRDP_PlayRemoteFxFile:
			settings->PlayRemoteFxFile = _strdup(param);
			break;

		case FreeRDP_GatewayHostname:
			settings->GatewayHostname = _strdup(param);
			break;

		case FreeRDP_GatewayUsername:
			settings->GatewayUsername = _strdup(param);
			break;

		case FreeRDP_GatewayPassword:
			settings->GatewayPassword = _strdup(param);
			break;

		case FreeRDP_GatewayDomain:
			settings->GatewayDomain = _strdup(param);
			break;

		case FreeRDP_RemoteApplicationName:
			settings->RemoteApplicationName = _strdup(param);
			break;

		case FreeRDP_RemoteApplicationIcon:
			settings->RemoteApplicationIcon = _strdup(param);
			break;

		case FreeRDP_RemoteApplicationProgram:
			settings->RemoteApplicationProgram = _strdup(param);
			break;

		case FreeRDP_RemoteApplicationFile:
			settings->RemoteApplicationFile = _strdup(param);
			break;

		case FreeRDP_RemoteApplicationGuid:
			settings->RemoteApplicationGuid = _strdup(param);
			break;

		case FreeRDP_RemoteApplicationCmdLine:
			settings->RemoteApplicationCmdLine = _strdup(param);
			break;

		case FreeRDP_ImeFileName:
			settings->ImeFileName = _strdup(param);
			break;

		case FreeRDP_DrivesToRedirect:
			settings->DrivesToRedirect = _strdup(param);
			break;

		default:
			return -1;
			break;
	}

	// Mark field as modified
	settings->settings_modified[id] = 1;

	EventArgsInit(&e, "freerdp");
	e.id = id;
	PubSub_OnParamChange(context->pubSub, context, &e);

	return 0;
}

double freerdp_get_param_double(rdpSettings* settings, int id)
{
	switch (id)
	{
		case FreeRDP_ScalingFactor:
			return settings->ScalingFactor;
			break;

		default:
			return 0;
			break;
	}

	return 0;
}

int freerdp_set_param_double(rdpSettings* settings, int id, double param)
{
	ParamChangeEventArgs e;
	rdpContext* context = ((freerdp*) settings->instance)->context;

	switch (id)
	{
		case FreeRDP_ScalingFactor:
			settings->ScalingFactor = param;
			break;

		default:
			return -1;
			break;
	}

	settings->settings_modified[id] = 1;

	EventArgsInit(&e, "freerdp");
	e.id = id;
	PubSub_OnParamChange(context->pubSub, context, &e);

	return 0;
}

