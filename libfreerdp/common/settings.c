/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Settings Management
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016 Armin Novak <armin.novak@gmail.com>
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
#include <freerdp/log.h>

#define TAG FREERDP_TAG("common")

int freerdp_addin_set_argument(ADDIN_ARGV* args, char* argument)
{
	int i;
	char **new_argv;

	for (i = 0; i < args->argc; i++)
	{
		if (strcmp(args->argv[i], argument) == 0)
		{
			return 1;
		}
	}

	new_argv = (char**) realloc(args->argv, sizeof(char*) * (args->argc + 1));
	if (!new_argv)
		return -1;
	args->argv = new_argv;
	args->argc++;
	if (!(args->argv[args->argc - 1] = _strdup(argument)))
		return -1;

	return 0;
}

int freerdp_addin_replace_argument(ADDIN_ARGV* args, char* previous, char* argument)
{
	int i;
	char **new_argv;

	for (i = 0; i < args->argc; i++)
	{
		if (strcmp(args->argv[i], previous) == 0)
		{
			free(args->argv[i]);
			if (!(args->argv[i] = _strdup(argument)))
				return -1;

			return 1;
		}
	}

	new_argv = (char**) realloc(args->argv, sizeof(char*) * (args->argc + 1));
	if (!new_argv)
		return -1;
	args->argv = new_argv;
	args->argc++;
	if (!(args->argv[args->argc - 1] = _strdup(argument)))
		return -1;

	return 0;
}

int freerdp_addin_set_argument_value(ADDIN_ARGV* args, char* option, char* value)
{
	int i;
	char* p;
	char* str;
	int length;
	char **new_argv;

	length = strlen(option) + strlen(value) + 1;
	str = (char*) malloc(length + 1);
	if (!str)
		return -1;
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

	new_argv = (char**) realloc(args->argv, sizeof(char*) * (args->argc + 1));
	if (!new_argv)
	{
		free(str);
		return -1;
	}

	args->argv = new_argv;
	args->argc++;
	args->argv[args->argc - 1] = str;

	return 0;
}

int freerdp_addin_replace_argument_value(ADDIN_ARGV* args, char* previous, char* option, char* value)
{
	int i;
	char* str;
	int length;
	char **new_argv;

	length = strlen(option) + strlen(value) + 1;
	str = (char*) malloc(length + 1);
	if (!str)
		return -1;
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

	new_argv = (char**) realloc(args->argv, sizeof(char*) * (args->argc + 1));
	if (!new_argv)
	{
		free(str);
		return -1;
	}
	args->argv = new_argv;
	args->argc++;
	args->argv[args->argc - 1] = str;

	return 0;
}

BOOL freerdp_device_collection_add(rdpSettings* settings, RDPDR_DEVICE* device)
{
	if (!settings->DeviceArray)
		return FALSE;

	if (settings->DeviceArraySize < (settings->DeviceCount + 1))
	{
		UINT32 new_size;
		RDPDR_DEVICE **new_array;

		new_size = settings->DeviceArraySize * 2;
		new_array = (RDPDR_DEVICE**)
				realloc(settings->DeviceArray, new_size * sizeof(RDPDR_DEVICE*));
		if (!new_array)
			return FALSE;
		settings->DeviceArray = new_array;
		settings->DeviceArraySize = new_size;
	}

	settings->DeviceArray[settings->DeviceCount++] = device;
	return TRUE;
}

RDPDR_DEVICE* freerdp_device_collection_find(rdpSettings* settings, const char* name)
{
	UINT32 index;
	RDPDR_DEVICE* device;

	for (index = 0; index < settings->DeviceCount; index++)
	{
		device = (RDPDR_DEVICE*) settings->DeviceArray[index];

		if (!device->Name)
			continue;

		if (strcmp(device->Name, name) == 0)
			return device;
	}

	return NULL;
}

RDPDR_DEVICE* freerdp_device_collection_find_type(rdpSettings* settings, UINT32 type)
{
	UINT32 index;
	RDPDR_DEVICE* device;

	for (index = 0; index < settings->DeviceCount; index++)
	{
		device = (RDPDR_DEVICE*) settings->DeviceArray[index];

		if (device->Type == type)
			return device;
	}

	return NULL;
}

RDPDR_DEVICE* freerdp_device_clone(RDPDR_DEVICE* device)
{
	if (device->Type == RDPDR_DTYP_FILESYSTEM)
	{
		RDPDR_DRIVE* drive = (RDPDR_DRIVE*) device;
		RDPDR_DRIVE* _drive = (RDPDR_DRIVE*) calloc(1, sizeof(RDPDR_DRIVE));

		if (!_drive)
			return NULL;

		_drive->Id = drive->Id;
		_drive->Type = drive->Type;

		_drive->Name = _strdup(drive->Name);
		if (!_drive->Name)
			goto out_fs_name_error;

		_drive->Path = _strdup(drive->Path);
		if (!_drive->Path)
			goto out_fs_path_error;

		return (RDPDR_DEVICE*) _drive;

out_fs_path_error:
		free(_drive->Name);
out_fs_name_error:
		free(_drive);
		return NULL;
	}

	if (device->Type == RDPDR_DTYP_PRINT)
	{
		RDPDR_PRINTER* printer = (RDPDR_PRINTER*) device;
		RDPDR_PRINTER* _printer = (RDPDR_PRINTER*) calloc(1, sizeof(RDPDR_PRINTER));

		if (!_printer)
			return NULL;

		_printer->Id = printer->Id;
		_printer->Type = printer->Type;

		if (printer->Name)
		{
			_printer->Name = _strdup(printer->Name);
			if (!_printer->Name)
				goto out_print_name_error;
		}

		if (printer->DriverName)
		{
			_printer->DriverName = _strdup(printer->DriverName);
			if (!_printer->DriverName)
				goto out_print_path_error;
		}

		return (RDPDR_DEVICE*) _printer;

out_print_path_error:
		free(_printer->Name);
out_print_name_error:
		free(_printer);
		return NULL;
	}

	if (device->Type == RDPDR_DTYP_SMARTCARD)
	{
		RDPDR_SMARTCARD* smartcard = (RDPDR_SMARTCARD*) device;
		RDPDR_SMARTCARD* _smartcard = (RDPDR_SMARTCARD*) calloc(1, sizeof(RDPDR_SMARTCARD));

		if (!_smartcard)
			return NULL;

		_smartcard->Id = smartcard->Id;
		_smartcard->Type = smartcard->Type;

		if (smartcard->Name)
		{
			_smartcard->Name = _strdup(smartcard->Name);
			if (!_smartcard->Name)
				goto out_smartc_name_error;
		}

		if (smartcard->Path)
		{
			_smartcard->Path = _strdup(smartcard->Path);
			if (!_smartcard->Path)
				goto out_smartc_path_error;
		}

		return (RDPDR_DEVICE*) _smartcard;

out_smartc_path_error:
		free(_smartcard->Name);
out_smartc_name_error:
		free(_smartcard);
		return NULL;
	}

	if (device->Type == RDPDR_DTYP_SERIAL)
	{
		RDPDR_SERIAL* serial = (RDPDR_SERIAL*) device;
		RDPDR_SERIAL* _serial = (RDPDR_SERIAL*) calloc(1, sizeof(RDPDR_SERIAL));

		if (!_serial)
			return NULL;

		_serial->Id = serial->Id;
		_serial->Type = serial->Type;

		if (serial->Name)
		{
			_serial->Name = _strdup(serial->Name);
			if (!_serial->Name)
				goto out_serial_name_error;
		}

		if (serial->Path)
		{
			_serial->Path = _strdup(serial->Path);
			if (!_serial->Path)
				goto out_serial_path_error;
		}

		if (serial->Driver)
		{
			_serial->Driver = _strdup(serial->Driver);
			if (!_serial->Driver)
				goto out_serial_driver_error;
		}

		return (RDPDR_DEVICE*) _serial;

out_serial_driver_error:
		free(_serial->Path);
out_serial_path_error:
		free(_serial->Name);
out_serial_name_error:
		free(_serial);
		return NULL;
	}

	if (device->Type == RDPDR_DTYP_PARALLEL)
	{
		RDPDR_PARALLEL* parallel = (RDPDR_PARALLEL*) device;
		RDPDR_PARALLEL* _parallel = (RDPDR_PARALLEL*) calloc(1, sizeof(RDPDR_PARALLEL));

		if (!_parallel)
			return NULL;

		_parallel->Id = parallel->Id;
		_parallel->Type = parallel->Type;

		if (parallel->Name)
		{
			_parallel->Name = _strdup(parallel->Name);
			if (!_parallel->Name)
				goto out_parallel_name_error;
		}

		if (parallel->Path)
		{
			_parallel->Path = _strdup(parallel->Path);
			if (!_parallel->Path)
				goto out_parallel_path_error;
		}

		return (RDPDR_DEVICE*) _parallel;
out_parallel_path_error:
		free(_parallel->Name);
out_parallel_name_error:
		free(_parallel);
		return NULL;

	}

	WLog_ERR(TAG, "unknown device type %"PRIu32"", device->Type);
	return NULL;
}

void freerdp_device_collection_free(rdpSettings* settings)
{
	UINT32 index;
	RDPDR_DEVICE* device;

	for (index = 0; index < settings->DeviceCount; index++)
	{
		device = (RDPDR_DEVICE*) settings->DeviceArray[index];

		if (!device)
			continue;

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
			free(((RDPDR_SERIAL*) device)->Driver);
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

BOOL freerdp_static_channel_collection_add(rdpSettings* settings, ADDIN_ARGV* channel)
{
	if (!settings->StaticChannelArray)
		return FALSE;

	if (settings->StaticChannelArraySize < (settings->StaticChannelCount + 1))
	{
		UINT32 new_size;
		ADDIN_ARGV **new_array;

		new_size = settings->StaticChannelArraySize * 2;
		new_array = (ADDIN_ARGV**)
				realloc(settings->StaticChannelArray, new_size * sizeof(ADDIN_ARGV*));
		if (!new_array)
			return FALSE;
		settings->StaticChannelArray = new_array;
		settings->StaticChannelArraySize = new_size;
	}

	settings->StaticChannelArray[settings->StaticChannelCount++] = channel;
	return TRUE;
}

ADDIN_ARGV* freerdp_static_channel_collection_find(rdpSettings* settings, const char* name)
{
	UINT32 index;
	ADDIN_ARGV* channel;

	for (index = 0; index < settings->StaticChannelCount; index++)
	{
		channel = settings->StaticChannelArray[index];

		if (strcmp(channel->argv[0], name) == 0)
			return channel;
	}

	return NULL;
}

ADDIN_ARGV* freerdp_static_channel_clone(ADDIN_ARGV* channel)
{
	int index;
	ADDIN_ARGV* _channel = NULL;

	_channel = (ADDIN_ARGV*) malloc(sizeof(ADDIN_ARGV));
	if (!_channel)
		return NULL;

	_channel->argc = channel->argc;
	_channel->argv = (char**) calloc(channel->argc, sizeof(char*));
	if (!_channel->argv)
		goto out_free;

	for (index = 0; index < _channel->argc; index++)
	{
		_channel->argv[index] = _strdup(channel->argv[index]);
		if (!_channel->argv[index])
			goto out_release_args;
	}

	return _channel;

out_release_args:
	for (index = 0; _channel->argv[index]; index++)
		free(_channel->argv[index]);
out_free:
	free(_channel);
	return NULL;
}

void freerdp_static_channel_collection_free(rdpSettings* settings)
{
	int j;
	UINT32 i;

	for (i = 0; i < settings->StaticChannelCount; i++)
	{
		if (!settings->StaticChannelArray[i])
			continue;

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

BOOL freerdp_dynamic_channel_collection_add(rdpSettings* settings, ADDIN_ARGV* channel)
{
	if (!settings->DynamicChannelArray)
		return FALSE;

	if (settings->DynamicChannelArraySize < (settings->DynamicChannelCount + 1))
	{
		ADDIN_ARGV **new_array;

		new_array = realloc(settings->DynamicChannelArray, settings->DynamicChannelArraySize * sizeof(ADDIN_ARGV*) * 2);
		if (!new_array)
			return FALSE;

		settings->DynamicChannelArraySize *= 2;
		settings->DynamicChannelArray = new_array;
	}

	settings->DynamicChannelArray[settings->DynamicChannelCount++] = channel;
	return TRUE;
}

ADDIN_ARGV* freerdp_dynamic_channel_collection_find(rdpSettings* settings, const char* name)
{
	UINT32 index;
	ADDIN_ARGV* channel;

	for (index = 0; index < settings->DynamicChannelCount; index++)
	{
		channel = settings->DynamicChannelArray[index];

		if (strcmp(channel->argv[0], name) == 0)
			return channel;
	}

	return NULL;
}

ADDIN_ARGV* freerdp_dynamic_channel_clone(ADDIN_ARGV* channel)
{
	int index;
	ADDIN_ARGV* _channel = NULL;

	_channel = (ADDIN_ARGV*) malloc(sizeof(ADDIN_ARGV));

	if (!_channel)
		return NULL;

	_channel->argc = channel->argc;
	_channel->argv = (char**) calloc(sizeof(char*), channel->argc);

	if (!_channel->argv)
		goto out_free;

	for (index = 0; index < _channel->argc; index++)
	{
		_channel->argv[index] = _strdup(channel->argv[index]);

		if (!_channel->argv[index])
			goto out_release_args;
	}

	return _channel;

out_release_args:
	for (index = 0; _channel->argv[index]; index++)
		free(_channel->argv[index]);
out_free:
	free(_channel);
	return NULL;
}

void freerdp_dynamic_channel_collection_free(rdpSettings* settings)
{
	int j;
	UINT32 i;

	for (i = 0; i < settings->DynamicChannelCount; i++)
	{
		if (!settings->DynamicChannelArray[i])
			continue;

		for (j = 0; j < settings->DynamicChannelArray[i]->argc; j++)
			free(settings->DynamicChannelArray[i]->argv[j]);

		free(settings->DynamicChannelArray[i]->argv);
		free(settings->DynamicChannelArray[i]);
	}

	free(settings->DynamicChannelArray);

	settings->DynamicChannelArraySize = 0;
	settings->DynamicChannelArray = NULL;
	settings->DynamicChannelCount = 0;
}

void freerdp_target_net_addresses_free(rdpSettings* settings)
{
	UINT32 index;

	for (index = 0; index < settings->TargetNetAddressCount; index++)
		free(settings->TargetNetAddresses[index]);

	free(settings->TargetNetAddresses);
	free(settings->TargetNetPorts);

	settings->TargetNetAddressCount = 0;
	settings->TargetNetAddresses = NULL;
	settings->TargetNetPorts = NULL;
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

void freerdp_set_gateway_usage_method(rdpSettings* settings, UINT32 GatewayUsageMethod)
{
	freerdp_set_param_uint32(settings, FreeRDP_GatewayUsageMethod, GatewayUsageMethod);

	if (GatewayUsageMethod == TSC_PROXY_MODE_NONE_DIRECT)
	{
		freerdp_set_param_bool(settings, FreeRDP_GatewayEnabled, FALSE);
		freerdp_set_param_bool(settings, FreeRDP_GatewayBypassLocal, FALSE);
	}
	else if (GatewayUsageMethod == TSC_PROXY_MODE_DIRECT)
	{
		freerdp_set_param_bool(settings, FreeRDP_GatewayEnabled, TRUE);
		freerdp_set_param_bool(settings, FreeRDP_GatewayBypassLocal, FALSE);
	}
	else if (GatewayUsageMethod == TSC_PROXY_MODE_DETECT)
	{
		freerdp_set_param_bool(settings, FreeRDP_GatewayEnabled, TRUE);
		freerdp_set_param_bool(settings, FreeRDP_GatewayBypassLocal, TRUE);
	}
	else if (GatewayUsageMethod == TSC_PROXY_MODE_DEFAULT)
	{
		/**
		 * This corresponds to "Automatically detect RD Gateway server settings",
		 * which means the client attempts to use gateway group policy settings
		 * http://technet.microsoft.com/en-us/library/cc770601.aspx
		 */
		freerdp_set_param_bool(settings, FreeRDP_GatewayEnabled, FALSE);
		freerdp_set_param_bool(settings, FreeRDP_GatewayBypassLocal, FALSE);
	}
	else if (GatewayUsageMethod == TSC_PROXY_MODE_NONE_DETECT)
	{
		freerdp_set_param_bool(settings, FreeRDP_GatewayEnabled, FALSE);
		freerdp_set_param_bool(settings, FreeRDP_GatewayBypassLocal, FALSE);
	}
}

void freerdp_update_gateway_usage_method(rdpSettings* settings, UINT32 GatewayEnabled, UINT32 GatewayBypassLocal)
{
	UINT32 GatewayUsageMethod = 0;

	if (!GatewayEnabled && !GatewayBypassLocal)
		GatewayUsageMethod = TSC_PROXY_MODE_NONE_DIRECT;
	else if (GatewayEnabled && !GatewayBypassLocal)
		GatewayUsageMethod = TSC_PROXY_MODE_DIRECT;
	else if (GatewayEnabled && GatewayBypassLocal)
		GatewayUsageMethod = TSC_PROXY_MODE_DETECT;

	freerdp_set_gateway_usage_method(settings, GatewayUsageMethod);
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

		case FreeRDP_NetworkAutoDetect:
			return settings->NetworkAutoDetect;

		case FreeRDP_SupportAsymetricKeys:
			return settings->SupportAsymetricKeys;

		case FreeRDP_SupportErrorInfoPdu:
			return settings->SupportErrorInfoPdu;

		case FreeRDP_SupportStatusInfoPdu:
			return settings->SupportStatusInfoPdu;

		case FreeRDP_SupportMonitorLayoutPdu:
			return settings->SupportMonitorLayoutPdu;

		case FreeRDP_SupportGraphicsPipeline:
			return settings->SupportGraphicsPipeline;

		case FreeRDP_SupportDynamicTimeZone:
			return settings->SupportDynamicTimeZone;

		case FreeRDP_UseRdpSecurityLayer:
			return settings->UseRdpSecurityLayer;

		case FreeRDP_ConsoleSession:
			return settings->ConsoleSession;

		case FreeRDP_SpanMonitors:
			return settings->SpanMonitors;

		case FreeRDP_UseMultimon:
			return settings->UseMultimon;

		case FreeRDP_ForceMultimon:
			return settings->ForceMultimon;

		case FreeRDP_AutoLogonEnabled:
			return settings->AutoLogonEnabled;

		case FreeRDP_CompressionEnabled:
			return settings->CompressionEnabled;

		case FreeRDP_DisableCtrlAltDel:
			return settings->DisableCtrlAltDel;

		case FreeRDP_EnableWindowsKey:
			return settings->EnableWindowsKey;

		case FreeRDP_MaximizeShell:
			return settings->MaximizeShell;

		case FreeRDP_LogonNotify:
			return settings->LogonNotify;

		case FreeRDP_LogonErrors:
			return settings->LogonErrors;

		case FreeRDP_MouseAttached:
			return settings->MouseAttached;

		case FreeRDP_MouseHasWheel:
			return settings->MouseHasWheel;

		case FreeRDP_RemoteConsoleAudio:
			return settings->RemoteConsoleAudio;

		case FreeRDP_AudioPlayback:
			return settings->AudioPlayback;

		case FreeRDP_AudioCapture:
			return settings->AudioCapture;

		case FreeRDP_VideoDisable:
			return settings->VideoDisable;

		case FreeRDP_PasswordIsSmartcardPin:
			return settings->PasswordIsSmartcardPin;

		case FreeRDP_UsingSavedCredentials:
			return settings->UsingSavedCredentials;

		case FreeRDP_ForceEncryptedCsPdu:
			return settings->ForceEncryptedCsPdu;

		case FreeRDP_HiDefRemoteApp:
			return settings->HiDefRemoteApp;

		case FreeRDP_IPv6Enabled:
			return settings->IPv6Enabled;

		case FreeRDP_AutoReconnectionEnabled:
			return settings->AutoReconnectionEnabled;

		case FreeRDP_DynamicDaylightTimeDisabled:
			return settings->DynamicDaylightTimeDisabled;

		case FreeRDP_AllowFontSmoothing:
			return settings->AllowFontSmoothing;

		case FreeRDP_DisableWallpaper:
			return settings->DisableWallpaper;

		case FreeRDP_DisableFullWindowDrag:
			return settings->DisableFullWindowDrag;

		case FreeRDP_DisableMenuAnims:
			return settings->DisableMenuAnims;

		case FreeRDP_DisableThemes:
			return settings->DisableThemes;

		case FreeRDP_DisableCursorShadow:
			return settings->DisableCursorShadow;

		case FreeRDP_DisableCursorBlinking:
			return settings->DisableCursorBlinking;

		case FreeRDP_AllowDesktopComposition:
			return settings->AllowDesktopComposition;

		case FreeRDP_RemoteAssistanceMode:
			return settings->RemoteAssistanceMode;

		case FreeRDP_TlsSecurity:
			return settings->TlsSecurity;

		case FreeRDP_NlaSecurity:
			return settings->NlaSecurity;

		case FreeRDP_RdpSecurity:
			return settings->RdpSecurity;

		case FreeRDP_ExtSecurity:
			return settings->ExtSecurity;

		case FreeRDP_Authentication:
			return settings->Authentication;

		case FreeRDP_NegotiateSecurityLayer:
			return settings->NegotiateSecurityLayer;

		case FreeRDP_RestrictedAdminModeRequired:
			return settings->RestrictedAdminModeRequired;

		case FreeRDP_DisableCredentialsDelegation:
			return settings->DisableCredentialsDelegation;

		case FreeRDP_AuthenticationLevel:
			return settings->AuthenticationLevel;

		case FreeRDP_VmConnectMode:
			return settings->VmConnectMode;

		case FreeRDP_MstscCookieMode:
			return settings->MstscCookieMode;

		case FreeRDP_SendPreconnectionPdu:
			return settings->SendPreconnectionPdu;

		case FreeRDP_IgnoreCertificate:
			return settings->IgnoreCertificate;

		case FreeRDP_AutoAcceptCertificate:
			return settings->AutoAcceptCertificate;

		case FreeRDP_ExternalCertificateManagement:
			return settings->ExternalCertificateManagement;

		case FreeRDP_FIPSMode:
			return settings->FIPSMode;

		case FreeRDP_Workarea:
			return settings->Workarea;

		case FreeRDP_Fullscreen:
			return settings->Fullscreen;

		case FreeRDP_GrabKeyboard:
			return settings->GrabKeyboard;

		case FreeRDP_Decorations:
			return settings->Decorations;

		case FreeRDP_SmartSizing:
			return settings->SmartSizing;

		case FreeRDP_MouseMotion:
			return settings->MouseMotion;

		case FreeRDP_AsyncInput:
			return settings->AsyncInput;

		case FreeRDP_AsyncUpdate:
			return settings->AsyncUpdate;

		case FreeRDP_AsyncChannels:
			return settings->AsyncChannels;

		case FreeRDP_AsyncTransport:
			return settings->AsyncTransport;

		case FreeRDP_ToggleFullscreen:
			return settings->ToggleFullscreen;

		case FreeRDP_SoftwareGdi:
			return settings->SoftwareGdi;

		case FreeRDP_LocalConnection:
			return settings->LocalConnection;

		case FreeRDP_AuthenticationOnly:
			return settings->AuthenticationOnly;

		case FreeRDP_CredentialsFromStdin:
			return settings->CredentialsFromStdin;

		case FreeRDP_DumpRemoteFx:
			return settings->DumpRemoteFx;

		case FreeRDP_PlayRemoteFx:
			return settings->PlayRemoteFx;

		case FreeRDP_GatewayUseSameCredentials:
			return settings->GatewayUseSameCredentials;

		case FreeRDP_GatewayEnabled:
			return settings->GatewayEnabled;

		case FreeRDP_GatewayBypassLocal:
			return settings->GatewayBypassLocal;

		case FreeRDP_GatewayRpcTransport:
			return settings->GatewayRpcTransport;

		case FreeRDP_GatewayHttpTransport:
			return settings->GatewayHttpTransport;

		case FreeRDP_GatewayUdpTransport:
			return settings->GatewayUdpTransport;

		case FreeRDP_RemoteApplicationMode:
			return settings->RemoteApplicationMode;

		case FreeRDP_DisableRemoteAppCapsCheck:
			return settings->DisableRemoteAppCapsCheck;

		case FreeRDP_RemoteAppLanguageBarSupported:
			return settings->RemoteAppLanguageBarSupported;

		case FreeRDP_RefreshRect:
			return settings->RefreshRect;

		case FreeRDP_SuppressOutput:
			return settings->SuppressOutput;

		case FreeRDP_FastPathOutput:
			return settings->FastPathOutput;

		case FreeRDP_SaltedChecksum:
			return settings->SaltedChecksum;

		case FreeRDP_LongCredentialsSupported:
			return settings->LongCredentialsSupported;

		case FreeRDP_NoBitmapCompressionHeader:
			return settings->NoBitmapCompressionHeader;

		case FreeRDP_BitmapCompressionDisabled:
			return settings->BitmapCompressionDisabled;

		case FreeRDP_DesktopResize:
			return settings->DesktopResize;

		case FreeRDP_DrawAllowDynamicColorFidelity:
			return settings->DrawAllowDynamicColorFidelity;

		case FreeRDP_DrawAllowColorSubsampling:
			return settings->DrawAllowColorSubsampling;

		case FreeRDP_DrawAllowSkipAlpha:
			return settings->DrawAllowSkipAlpha;

		case FreeRDP_BitmapCacheV3Enabled:
			return settings->BitmapCacheV3Enabled;

		case FreeRDP_AltSecFrameMarkerSupport:
			return settings->AltSecFrameMarkerSupport;

		case FreeRDP_BitmapCacheEnabled:
			return settings->BitmapCacheEnabled;

		case FreeRDP_AllowCacheWaitingList:
			return settings->AllowCacheWaitingList;

		case FreeRDP_BitmapCachePersistEnabled:
			return settings->BitmapCachePersistEnabled;

		case FreeRDP_ColorPointerFlag:
			return settings->ColorPointerFlag;

		case FreeRDP_UnicodeInput:
			return settings->UnicodeInput;

		case FreeRDP_FastPathInput:
			return settings->FastPathInput;

		case FreeRDP_MultiTouchInput:
			return settings->MultiTouchInput;

		case FreeRDP_MultiTouchGestures:
			return settings->MultiTouchGestures;

		case FreeRDP_SoundBeepsEnabled:
			return settings->SoundBeepsEnabled;

		case FreeRDP_SurfaceCommandsEnabled:
			return settings->SurfaceCommandsEnabled;

		case FreeRDP_FrameMarkerCommandEnabled:
			return settings->FrameMarkerCommandEnabled;

		case FreeRDP_RemoteFxOnly:
			return settings->RemoteFxOnly;

		case FreeRDP_RemoteFxCodec:
			return settings->RemoteFxCodec;

		case FreeRDP_RemoteFxImageCodec:
			return settings->RemoteFxImageCodec;

		case FreeRDP_NSCodec:
			return settings->NSCodec;

		case FreeRDP_NSCodecAllowSubsampling:
			return settings->NSCodecAllowSubsampling;

		case FreeRDP_NSCodecAllowDynamicColorFidelity:
			return settings->NSCodecAllowDynamicColorFidelity;

		case FreeRDP_JpegCodec:
			return settings->JpegCodec;

		case FreeRDP_GfxThinClient:
			return settings->GfxThinClient;

		case FreeRDP_GfxSmallCache:
			return settings->GfxSmallCache;

		case FreeRDP_GfxProgressive:
			return settings->GfxProgressive;

		case FreeRDP_GfxProgressiveV2:
			return settings->GfxProgressiveV2;

		case FreeRDP_GfxH264:
			return settings->GfxH264;

		case FreeRDP_GfxAVC444:
			return settings->GfxAVC444;

		case FreeRDP_DrawNineGridEnabled:
			return settings->DrawNineGridEnabled;

		case FreeRDP_DrawGdiPlusEnabled:
			return settings->DrawGdiPlusEnabled;

		case FreeRDP_DrawGdiPlusCacheEnabled:
			return settings->DrawGdiPlusCacheEnabled;

		case FreeRDP_DeviceRedirection:
			return settings->DeviceRedirection;

		case FreeRDP_RedirectDrives:
			return settings->RedirectDrives;

		case FreeRDP_RedirectHomeDrive:
			return settings->RedirectHomeDrive;

		case FreeRDP_RedirectSmartCards:
			return settings->RedirectSmartCards;

		case FreeRDP_RedirectPrinters:
			return settings->RedirectPrinters;

		case FreeRDP_RedirectSerialPorts:
			return settings->RedirectSerialPorts;

		case FreeRDP_RedirectParallelPorts:
			return settings->RedirectParallelPorts;

		case FreeRDP_RedirectClipboard:
			return settings->RedirectClipboard;

		default:
			WLog_ERR(TAG,  "freerdp_get_param_bool: unknown id: %d", id);
			return -1;
	}
}

int freerdp_set_param_bool(rdpSettings* settings, int id, BOOL param)
{
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

		case FreeRDP_UseRdpSecurityLayer:
			settings->UseRdpSecurityLayer = param;
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

		case FreeRDP_HiDefRemoteApp:
			settings->HiDefRemoteApp = param;
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

		case FreeRDP_RemoteAssistanceMode:
			settings->RemoteAssistanceMode = param;
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

		case FreeRDP_RestrictedAdminModeRequired:
			settings->RestrictedAdminModeRequired = param;
			break;

		case FreeRDP_DisableCredentialsDelegation:
			settings->DisableCredentialsDelegation = param;
			break;

		case FreeRDP_AuthenticationLevel:
			settings->AuthenticationLevel = param;
			break;

		case FreeRDP_VmConnectMode:
			settings->VmConnectMode = param;
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

		case FreeRDP_AutoAcceptCertificate:
			settings->AutoAcceptCertificate = param;
			break;

		case FreeRDP_ExternalCertificateManagement:
			settings->ExternalCertificateManagement = param;
			break;

		case FreeRDP_FIPSMode:
			settings->FIPSMode = param;
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

		case FreeRDP_AsyncTransport:
			settings->AsyncTransport = param;
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

		case FreeRDP_GatewayBypassLocal:
			settings->GatewayBypassLocal = param;
			break;

		case FreeRDP_GatewayRpcTransport:
			settings->GatewayRpcTransport = param;
			break;

		case FreeRDP_GatewayHttpTransport:
			settings->GatewayHttpTransport = param;
			break;

		case FreeRDP_GatewayUdpTransport:
			settings->GatewayUdpTransport = param;
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

		case FreeRDP_NSCodecAllowSubsampling:
			settings->NSCodecAllowSubsampling = param;
			break;

		case FreeRDP_NSCodecAllowDynamicColorFidelity:
			settings->NSCodecAllowDynamicColorFidelity = param;
			break;

		case FreeRDP_JpegCodec:
			settings->JpegCodec = param;
			break;

		case FreeRDP_GfxThinClient:
			settings->GfxThinClient = param;
			break;

		case FreeRDP_GfxSmallCache:
			settings->GfxSmallCache = param;
			break;

		case FreeRDP_GfxProgressive:
			settings->GfxProgressive = param;
			break;

		case FreeRDP_GfxProgressiveV2:
			settings->GfxProgressiveV2 = param;
			break;

		case FreeRDP_GfxH264:
			settings->GfxH264 = param;
			break;

		case FreeRDP_GfxAVC444:
			settings->GfxAVC444 = param;
			break;

		case FreeRDP_GfxSendQoeAck:
			settings->GfxSendQoeAck = param;
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
			WLog_ERR(TAG,  "freerdp_set_param_bool: unknown id %d (param = %"PRId32")", id, param);
			return -1;
	}

	/* Mark field as modified */
	settings->SettingsModified[id] = 1;

	return -1;
}

int freerdp_get_param_int(rdpSettings* settings, int id)
{
	switch (id)
	{
		case FreeRDP_XPan:
			return settings->XPan;

		case FreeRDP_YPan:
			return settings->YPan;

		default:
			WLog_ERR(TAG,  "freerdp_get_param_int: unknown id: %d", id);
			return 0;
	}
}

int freerdp_set_param_int(rdpSettings* settings, int id, int param)
{
	switch (id)
	{
		case FreeRDP_XPan:
			settings->XPan = param;
			break;

		case FreeRDP_YPan:
			settings->YPan = param;
			break;

		default:
			WLog_ERR(TAG,  "freerdp_set_param_int: unknown id %d (param = %d)", id, param);
			return -1;
	}

	settings->SettingsModified[id] = 1;

	return 0;
}

UINT32 freerdp_get_param_uint32(rdpSettings* settings, int id)
{
	switch (id)
	{
		case FreeRDP_ShareId:
			return settings->ShareId;

		case FreeRDP_PduSource:
			return settings->PduSource;

		case FreeRDP_ServerPort:
			return settings->ServerPort;

		case FreeRDP_RdpVersion:
			return settings->RdpVersion;

		case FreeRDP_DesktopWidth:
			return settings->DesktopWidth;

		case FreeRDP_DesktopHeight:
			return settings->DesktopHeight;

		case FreeRDP_ColorDepth:
			return settings->ColorDepth;

		case FreeRDP_ConnectionType:
			return settings->ConnectionType;

		case FreeRDP_ClientBuild:
			return settings->ClientBuild;

		case FreeRDP_EarlyCapabilityFlags:
			return settings->EarlyCapabilityFlags;

		case FreeRDP_EncryptionMethods:
			return settings->EncryptionMethods;

		case FreeRDP_ExtEncryptionMethods:
			return settings->ExtEncryptionMethods;

		case FreeRDP_EncryptionLevel:
			return settings->EncryptionLevel;

		case FreeRDP_ServerRandomLength:
			return settings->ServerRandomLength;

		case FreeRDP_ClientRandomLength:
			return settings->ClientRandomLength;

		case FreeRDP_ChannelCount:
			return settings->ChannelCount;

		case FreeRDP_ChannelDefArraySize:
			return settings->ChannelDefArraySize;

		case FreeRDP_ClusterInfoFlags:
			return settings->ClusterInfoFlags;

		case FreeRDP_RedirectedSessionId:
			return settings->RedirectedSessionId;

		case FreeRDP_MonitorDefArraySize:
			return settings->MonitorDefArraySize;

		case FreeRDP_DesktopPosX:
			return settings->DesktopPosX;

		case FreeRDP_DesktopPosY:
			return settings->DesktopPosY;

		case FreeRDP_MultitransportFlags:
			return settings->MultitransportFlags;

		case FreeRDP_CompressionLevel:
			return settings->CompressionLevel;

		case FreeRDP_AutoReconnectMaxRetries:
			return settings->AutoReconnectMaxRetries;

		case FreeRDP_PerformanceFlags:
			return settings->PerformanceFlags;

		case FreeRDP_RequestedProtocols:
			return settings->RequestedProtocols;

		case FreeRDP_SelectedProtocol:
			return settings->SelectedProtocol;

		case FreeRDP_NegotiationFlags:
			return settings->NegotiationFlags;

		case FreeRDP_CookieMaxLength:
			return settings->CookieMaxLength;

		case FreeRDP_PreconnectionId:
			return settings->PreconnectionId;

		case FreeRDP_RedirectionFlags:
			return settings->RedirectionFlags;

		case FreeRDP_LoadBalanceInfoLength:
			return settings->LoadBalanceInfoLength;

		case FreeRDP_RedirectionPasswordLength:
			return settings->RedirectionPasswordLength;

		case FreeRDP_RedirectionTsvUrlLength:
			return settings->RedirectionTsvUrlLength;

		case FreeRDP_TargetNetAddressCount:
			return settings->TargetNetAddressCount;

		case FreeRDP_PercentScreen:
			return settings->PercentScreen;

		case FreeRDP_PercentScreenUseWidth:
			return settings->PercentScreenUseWidth;

		case FreeRDP_PercentScreenUseHeight:
			return settings->PercentScreenUseHeight;

		case FreeRDP_GatewayUsageMethod:
			return settings->GatewayUsageMethod;

		case FreeRDP_GatewayPort:
			return settings->GatewayPort;

		case FreeRDP_GatewayCredentialsSource:
			return settings->GatewayCredentialsSource;

		case FreeRDP_ProxyType:
			return settings->ProxyType;

		case FreeRDP_ProxyPort:
			return settings->ProxyPort;

		case FreeRDP_RemoteAppNumIconCaches:
			return settings->RemoteAppNumIconCaches;

		case FreeRDP_RemoteAppNumIconCacheEntries:
			return settings->RemoteAppNumIconCacheEntries;

		case FreeRDP_ReceivedCapabilitiesSize:
			return settings->ReceivedCapabilitiesSize;

		case FreeRDP_OsMajorType:
			return settings->OsMajorType;

		case FreeRDP_OsMinorType:
			return settings->OsMinorType;

		case FreeRDP_BitmapCacheVersion:
			return settings->BitmapCacheVersion;

		case FreeRDP_BitmapCacheV2NumCells:
			return settings->BitmapCacheV2NumCells;

		case FreeRDP_PointerCacheSize:
			return settings->PointerCacheSize;

		case FreeRDP_KeyboardLayout:
			return settings->KeyboardLayout;

		case FreeRDP_KeyboardType:
			return settings->KeyboardType;

		case FreeRDP_KeyboardSubType:
			return settings->KeyboardSubType;

		case FreeRDP_KeyboardFunctionKey:
			return settings->KeyboardFunctionKey;

		case FreeRDP_KeyboardHook:
			return settings->KeyboardHook;
			break;

		case FreeRDP_BrushSupportLevel:
			return settings->BrushSupportLevel;

		case FreeRDP_GlyphSupportLevel:
			return settings->GlyphSupportLevel;

		case FreeRDP_OffscreenSupportLevel:
			return settings->OffscreenSupportLevel;

		case FreeRDP_OffscreenCacheSize:
			return settings->OffscreenCacheSize;

		case FreeRDP_OffscreenCacheEntries:
			return settings->OffscreenCacheEntries;

		case FreeRDP_VirtualChannelCompressionFlags:
			return settings->VirtualChannelCompressionFlags;

		case FreeRDP_VirtualChannelChunkSize:
			return settings->VirtualChannelChunkSize;

		case FreeRDP_MultifragMaxRequestSize:
			return settings->MultifragMaxRequestSize;

		case FreeRDP_LargePointerFlag:
			return settings->LargePointerFlag;

		case FreeRDP_CompDeskSupportLevel:
			return settings->CompDeskSupportLevel;

		case FreeRDP_RemoteFxCodecId:
			return settings->RemoteFxCodecId;

		case FreeRDP_RemoteFxCodecMode:
			return settings->RemoteFxCodecMode;

		case FreeRDP_NSCodecId:
			return settings->NSCodecId;

		case FreeRDP_FrameAcknowledge:
			return settings->FrameAcknowledge;

		case FreeRDP_NSCodecColorLossLevel:
			return settings->NSCodecColorLossLevel;

		case FreeRDP_JpegCodecId:
			return settings->JpegCodecId;

		case FreeRDP_JpegQuality:
			return settings->JpegQuality;

		case FreeRDP_BitmapCacheV3CodecId:
			return settings->BitmapCacheV3CodecId;

		case FreeRDP_DrawNineGridCacheSize:
			return settings->DrawNineGridCacheSize;

		case FreeRDP_DrawNineGridCacheEntries:
			return settings->DrawNineGridCacheEntries;

		case FreeRDP_DeviceCount:
			return settings->DeviceCount;

		case FreeRDP_DeviceArraySize:
			return settings->DeviceArraySize;

		case FreeRDP_StaticChannelCount:
			return settings->StaticChannelCount;

		case FreeRDP_StaticChannelArraySize:
			return settings->StaticChannelArraySize;

		case FreeRDP_DynamicChannelCount:
			return settings->DynamicChannelCount;

		case FreeRDP_DynamicChannelArraySize:
			return settings->DynamicChannelArraySize;

		case FreeRDP_SmartSizingWidth:
			return settings->SmartSizingWidth;

		case FreeRDP_SmartSizingHeight:
			return settings->SmartSizingHeight;

		default:
			WLog_ERR(TAG,  "freerdp_get_param_uint32: unknown id: %d", id);
			return 0;
	}
}

int freerdp_set_param_uint32(rdpSettings* settings, int id, UINT32 param)
{
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

		case FreeRDP_ServerRandomLength:
			settings->ServerRandomLength = param;
			break;

		case FreeRDP_ClientRandomLength:
			settings->ClientRandomLength = param;
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

		case FreeRDP_CompressionLevel:
			settings->CompressionLevel = param;
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

		case FreeRDP_RedirectionPasswordLength:
			settings->RedirectionPasswordLength = param;
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

		case FreeRDP_PercentScreenUseWidth:
			settings->PercentScreenUseWidth = param;
			break;

		case FreeRDP_PercentScreenUseHeight:
			settings->PercentScreenUseHeight = param;
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

		case FreeRDP_ProxyType:
			settings->ProxyType = param;
			break;

		case FreeRDP_ProxyPort:
			settings->ProxyPort = param;
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

		case FreeRDP_KeyboardHook:
			settings->KeyboardHook = param;
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

		case FreeRDP_FrameAcknowledge:
			settings->FrameAcknowledge = param;
			break;

		case FreeRDP_NSCodecColorLossLevel:
			settings->NSCodecColorLossLevel = param;
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
			WLog_ERR(TAG, "freerdp_set_param_uint32: unknown id %d (param = %"PRIu32")", id, param);
			return -1;
	}

	/* Mark field as modified */
	settings->SettingsModified[id] = 1;

	return 0;
}

UINT64 freerdp_get_param_uint64(rdpSettings* settings, int id)
{
	switch (id)
	{
		case FreeRDP_ParentWindowId:
			return settings->ParentWindowId;

		default:
			WLog_ERR(TAG, "freerdp_get_param_uint64: unknown id: %d", id);
			return -1;
	}
}

int freerdp_set_param_uint64(rdpSettings* settings, int id, UINT64 param)
{
	switch (id)
	{
		case FreeRDP_ParentWindowId:
			settings->ParentWindowId = param;
			break;

		default:
			WLog_ERR(TAG,  "freerdp_set_param_uint64: unknown id %d (param = %"PRIu64")", id, param);
			return -1;
	}

	/* Mark field as modified */
	settings->SettingsModified[id] = 1;

	return 0;
}

char* freerdp_get_param_string(rdpSettings* settings, int id)
{
	switch (id)
	{
		case FreeRDP_ServerHostname:
			return settings->ServerHostname;

		case FreeRDP_Username:
			return settings->Username;

		case FreeRDP_Password:
			return settings->Password;

		case FreeRDP_Domain:
			return settings->Domain;

		case FreeRDP_PasswordHash:
			return settings->PasswordHash;

		case FreeRDP_ClientHostname:
			return settings->ClientHostname;

		case FreeRDP_ClientProductId:
			return settings->ClientProductId;

		case FreeRDP_AlternateShell:
			return settings->AlternateShell;

		case FreeRDP_ShellWorkingDirectory:
			return settings->ShellWorkingDirectory;

		case FreeRDP_ClientAddress:
			return settings->ClientAddress;

		case FreeRDP_ClientDir:
			return settings->ClientDir;

		case FreeRDP_DynamicDSTTimeZoneKeyName:
			return settings->DynamicDSTTimeZoneKeyName;

		case FreeRDP_RemoteAssistanceSessionId:
			return settings->RemoteAssistanceSessionId;

		case FreeRDP_RemoteAssistancePassStub:
			return settings->RemoteAssistancePassStub;

		case FreeRDP_RemoteAssistancePassword:
			return settings->RemoteAssistancePassword;

		case FreeRDP_RemoteAssistanceRCTicket:
			return settings->RemoteAssistanceRCTicket;

		case FreeRDP_AuthenticationServiceClass:
			return settings->AuthenticationServiceClass;

		case FreeRDP_AllowedTlsCiphers:
			return settings->AllowedTlsCiphers;

		case FreeRDP_NtlmSamFile:
			return settings->NtlmSamFile;

		case FreeRDP_PreconnectionBlob:
			return settings->PreconnectionBlob;

		case FreeRDP_KerberosKdc:
			return settings->KerberosKdc;

		case FreeRDP_KerberosRealm:
			return settings->KerberosRealm;

		case FreeRDP_CertificateName:
			return settings->CertificateName;

		case FreeRDP_CertificateFile:
			return settings->CertificateFile;

		case FreeRDP_PrivateKeyFile:
			return settings->PrivateKeyFile;

		case FreeRDP_RdpKeyFile:
			return settings->RdpKeyFile;

		case FreeRDP_CertificateContent:
			return settings->CertificateContent;

		case FreeRDP_PrivateKeyContent:
			return settings->PrivateKeyContent;

		case FreeRDP_RdpKeyContent:
			return settings->RdpKeyContent;

		case FreeRDP_WindowTitle:
			return settings->WindowTitle;

		case FreeRDP_ComputerName:
			return settings->ComputerName;

		case FreeRDP_ConnectionFile:
			return settings->ConnectionFile;

		case FreeRDP_AssistanceFile:
			return settings->AssistanceFile;

		case FreeRDP_HomePath:
			return settings->HomePath;

		case FreeRDP_ConfigPath:
			return settings->ConfigPath;

		case FreeRDP_CurrentPath:
			return settings->CurrentPath;

		case FreeRDP_DumpRemoteFxFile:
			return settings->DumpRemoteFxFile;

		case FreeRDP_PlayRemoteFxFile:
			return settings->PlayRemoteFxFile;

		case FreeRDP_GatewayHostname:
			return settings->GatewayHostname;

		case FreeRDP_GatewayUsername:
			return settings->GatewayUsername;

		case FreeRDP_GatewayPassword:
			return settings->GatewayPassword;

		case FreeRDP_GatewayDomain:
			return settings->GatewayDomain;

		case FreeRDP_ProxyHostname:
			return settings->ProxyHostname;

		case FreeRDP_RemoteApplicationName:
			return settings->RemoteApplicationName;

		case FreeRDP_RemoteApplicationIcon:
			return settings->RemoteApplicationIcon;

		case FreeRDP_RemoteApplicationProgram:
			return settings->RemoteApplicationProgram;

		case FreeRDP_RemoteApplicationFile:
			return settings->RemoteApplicationFile;

		case FreeRDP_RemoteApplicationGuid:
			return settings->RemoteApplicationGuid;

		case FreeRDP_RemoteApplicationCmdLine:
			return settings->RemoteApplicationCmdLine;

		case FreeRDP_ImeFileName:
			return settings->ImeFileName;

		case FreeRDP_DrivesToRedirect:
			return settings->DrivesToRedirect;

		default:
			WLog_ERR(TAG, "freerdp_get_param_string: unknown id: %d", id);
			return NULL;
	}
}

int freerdp_set_param_string(rdpSettings* settings, int id, const char* param)
{
	char **tmp = NULL;

	if (!param)
		return -1;

	switch (id)
	{
		case FreeRDP_ServerHostname:
			tmp = &settings->ServerHostname;
			break;

		case FreeRDP_Username:
			tmp = &settings->Username;
			break;

		case FreeRDP_Password:
			tmp = &settings->Password;
			break;

		case FreeRDP_Domain:
			tmp = &settings->Domain;
			break;

		case FreeRDP_PasswordHash:
			tmp = &settings->PasswordHash;
			break;

		case FreeRDP_ClientHostname:
			tmp = &settings->ClientHostname;
			break;

		case FreeRDP_ClientProductId:
			tmp = &settings->ClientProductId;
			break;

		case FreeRDP_AlternateShell:
			tmp = &settings->AlternateShell;
			break;

		case FreeRDP_ShellWorkingDirectory:
			tmp = &settings->ShellWorkingDirectory;
			break;

		case FreeRDP_ClientAddress:
			tmp = &settings->ClientAddress;
			break;

		case FreeRDP_ClientDir:
			tmp = &settings->ClientDir;
			break;

		case FreeRDP_DynamicDSTTimeZoneKeyName:
			tmp = &settings->DynamicDSTTimeZoneKeyName;
			break;

		case FreeRDP_RemoteAssistanceSessionId:
			tmp = &settings->RemoteAssistanceSessionId;
			break;

		case FreeRDP_RemoteAssistancePassStub:
			tmp = &settings->RemoteAssistancePassStub;
			break;

		case FreeRDP_RemoteAssistancePassword:
			tmp = &settings->RemoteAssistancePassword;
			break;

		case FreeRDP_RemoteAssistanceRCTicket:
			tmp = &settings->RemoteAssistanceRCTicket;
			break;

		case FreeRDP_AuthenticationServiceClass:
			tmp = &settings->AuthenticationServiceClass;
			break;

		case FreeRDP_AllowedTlsCiphers:
			tmp = &settings->AllowedTlsCiphers;
			break;

		case FreeRDP_NtlmSamFile:
			tmp = &settings->NtlmSamFile;
			break;

		case FreeRDP_PreconnectionBlob:
			tmp = &settings->PreconnectionBlob;
			break;

		case FreeRDP_KerberosKdc:
			tmp = &settings->KerberosKdc;
			break;

		case FreeRDP_KerberosRealm:
			tmp = &settings->KerberosRealm;
			break;

		case FreeRDP_CertificateName:
			tmp = &settings->CertificateName;
			break;

		case FreeRDP_CertificateFile:
			tmp = &settings->CertificateFile;
			break;

		case FreeRDP_PrivateKeyFile:
			tmp = &settings->PrivateKeyFile;
			break;

		case FreeRDP_CertificateContent:
			tmp = &settings->CertificateContent;
			break;

		case FreeRDP_PrivateKeyContent:
			tmp = &settings->PrivateKeyContent;
			break;

		case FreeRDP_RdpKeyContent:
			tmp = &settings->RdpKeyContent;
			break;

		case FreeRDP_RdpKeyFile:
			tmp = &settings->RdpKeyFile;
			break;

		case FreeRDP_WindowTitle:
			tmp = &settings->WindowTitle;
			break;

		case FreeRDP_ComputerName:
			tmp = &settings->ComputerName;
			break;

		case FreeRDP_ConnectionFile:
			tmp = &settings->ConnectionFile;
			break;

		case FreeRDP_AssistanceFile:
			tmp = &settings->AssistanceFile;
			break;

		case FreeRDP_HomePath:
			tmp = &settings->HomePath;
			break;

		case FreeRDP_ConfigPath:
			tmp = &settings->ConfigPath;
			break;

		case FreeRDP_CurrentPath:
			tmp = &settings->CurrentPath;
			break;

		case FreeRDP_DumpRemoteFxFile:
			tmp = &settings->DumpRemoteFxFile;
			break;

		case FreeRDP_PlayRemoteFxFile:
			tmp = &settings->PlayRemoteFxFile;
			break;

		case FreeRDP_GatewayHostname:
			tmp = &settings->GatewayHostname;
			break;

		case FreeRDP_GatewayUsername:
			tmp = &settings->GatewayUsername;
			break;

		case FreeRDP_GatewayPassword:
			tmp = &settings->GatewayPassword;
			break;

		case FreeRDP_GatewayDomain:
			tmp = &settings->GatewayDomain;
			break;

		case FreeRDP_ProxyHostname:
			tmp = &settings->ProxyHostname;
			break;

		case FreeRDP_RemoteApplicationName:
			tmp = &settings->RemoteApplicationName;
			break;

		case FreeRDP_RemoteApplicationIcon:
			tmp = &settings->RemoteApplicationIcon;
			break;

		case FreeRDP_RemoteApplicationProgram:
			tmp = &settings->RemoteApplicationProgram;
			break;

		case FreeRDP_RemoteApplicationFile:
			tmp = &settings->RemoteApplicationFile;
			break;

		case FreeRDP_RemoteApplicationGuid:
			tmp = &settings->RemoteApplicationGuid;
			break;

		case FreeRDP_RemoteApplicationCmdLine:
			tmp = &settings->RemoteApplicationCmdLine;
			break;

		case FreeRDP_ImeFileName:
			tmp = &settings->ImeFileName;
			break;

		case FreeRDP_DrivesToRedirect:
			tmp = &settings->DrivesToRedirect;
			break;

		default:
			WLog_ERR(TAG, "unknown id %d (param = %s)", id, param);
			return -1;
	}

	free(*tmp);
	if (!(*tmp = _strdup(param)))
		return -1;

	/* Mark field as modified */
	settings->SettingsModified[id] = 1;

	return 0;
}
