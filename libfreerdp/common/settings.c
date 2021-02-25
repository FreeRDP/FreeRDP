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
#include <errno.h>

#include <winpr/crt.h>

#include "../core/settings.h"
#include "../core/certificate.h"
#include <freerdp/settings.h>
#include <freerdp/freerdp.h>
#include <freerdp/log.h>

#define TAG FREERDP_TAG("common")

int freerdp_addin_set_argument(ADDIN_ARGV* args, char* argument)
{
	int i;
	char** new_argv;

	for (i = 0; i < args->argc; i++)
	{
		if (strcmp(args->argv[i], argument) == 0)
		{
			return 1;
		}
	}

	new_argv = (char**)realloc(args->argv, sizeof(char*) * (args->argc + 1));

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
	char** new_argv;

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

	new_argv = (char**)realloc(args->argv, sizeof(char*) * (args->argc + 1));

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
	size_t length;
	char** new_argv;
	length = strlen(option) + strlen(value) + 1;
	str = (char*)malloc(length + 1);

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

	new_argv = (char**)realloc(args->argv, sizeof(char*) * (args->argc + 1));

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

int freerdp_addin_replace_argument_value(ADDIN_ARGV* args, char* previous, char* option,
                                         char* value)
{
	int i;
	char* str;
	size_t length;
	char** new_argv;
	length = strlen(option) + strlen(value) + 1;
	str = (char*)malloc(length + 1);

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

	new_argv = (char**)realloc(args->argv, sizeof(char*) * (args->argc + 1));

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

	if (freerdp_settings_get_uint32(settings, FreeRDP_DeviceArraySize) <
	    (settings->DeviceCount + 1))
	{
		UINT32 new_size;
		RDPDR_DEVICE** new_array;
		new_size = freerdp_settings_get_uint32(settings, FreeRDP_DeviceArraySize) * 2;
		new_array =
		    (RDPDR_DEVICE**)realloc(settings->DeviceArray, new_size * sizeof(RDPDR_DEVICE*));

		if (!new_array)
			return FALSE;

		settings->DeviceArray = new_array;
		if (!freerdp_settings_set_uint32(settings, FreeRDP_DeviceArraySize, new_size))
			return FALSE;
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
		device = (RDPDR_DEVICE*)settings->DeviceArray[index];

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
		device = (RDPDR_DEVICE*)settings->DeviceArray[index];

		if (device->Type == type)
			return device;
	}

	return NULL;
}

RDPDR_DEVICE* freerdp_device_clone(RDPDR_DEVICE* device)
{
	if (device->Type == RDPDR_DTYP_FILESYSTEM)
	{
		RDPDR_DRIVE* drive = (RDPDR_DRIVE*)device;
		RDPDR_DRIVE* _drive = (RDPDR_DRIVE*)calloc(1, sizeof(RDPDR_DRIVE));

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

		return (RDPDR_DEVICE*)_drive;
	out_fs_path_error:
		free(_drive->Name);
	out_fs_name_error:
		free(_drive);
		return NULL;
	}

	if (device->Type == RDPDR_DTYP_PRINT)
	{
		RDPDR_PRINTER* printer = (RDPDR_PRINTER*)device;
		RDPDR_PRINTER* _printer = (RDPDR_PRINTER*)calloc(1, sizeof(RDPDR_PRINTER));

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

		return (RDPDR_DEVICE*)_printer;
	out_print_path_error:
		free(_printer->Name);
	out_print_name_error:
		free(_printer);
		return NULL;
	}

	if (device->Type == RDPDR_DTYP_SMARTCARD)
	{
		RDPDR_SMARTCARD* smartcard = (RDPDR_SMARTCARD*)device;
		RDPDR_SMARTCARD* _smartcard = (RDPDR_SMARTCARD*)calloc(1, sizeof(RDPDR_SMARTCARD));

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

		return (RDPDR_DEVICE*)_smartcard;
	out_smartc_name_error:
		free(_smartcard);
		return NULL;
	}

	if (device->Type == RDPDR_DTYP_SERIAL)
	{
		RDPDR_SERIAL* serial = (RDPDR_SERIAL*)device;
		RDPDR_SERIAL* _serial = (RDPDR_SERIAL*)calloc(1, sizeof(RDPDR_SERIAL));

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

		return (RDPDR_DEVICE*)_serial;
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
		RDPDR_PARALLEL* parallel = (RDPDR_PARALLEL*)device;
		RDPDR_PARALLEL* _parallel = (RDPDR_PARALLEL*)calloc(1, sizeof(RDPDR_PARALLEL));

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

		return (RDPDR_DEVICE*)_parallel;
	out_parallel_path_error:
		free(_parallel->Name);
	out_parallel_name_error:
		free(_parallel);
		return NULL;
	}

	WLog_ERR(TAG, "unknown device type %" PRIu32 "", device->Type);
	return NULL;
}

void freerdp_device_collection_free(rdpSettings* settings)
{
	UINT32 index;
	RDPDR_DEVICE* device;

	for (index = 0; index < settings->DeviceCount; index++)
	{
		device = (RDPDR_DEVICE*)settings->DeviceArray[index];

		if (!device)
			continue;

		free(device->Name);

		if (settings->DeviceArray[index]->Type == RDPDR_DTYP_FILESYSTEM)
		{
			free(((RDPDR_DRIVE*)device)->Path);
		}
		else if (settings->DeviceArray[index]->Type == RDPDR_DTYP_PRINT)
		{
		}
		else if (settings->DeviceArray[index]->Type == RDPDR_DTYP_SMARTCARD)
		{
		}
		else if (settings->DeviceArray[index]->Type == RDPDR_DTYP_SERIAL)
		{
			free(((RDPDR_SERIAL*)device)->Path);
			free(((RDPDR_SERIAL*)device)->Driver);
		}
		else if (settings->DeviceArray[index]->Type == RDPDR_DTYP_PARALLEL)
		{
			free(((RDPDR_PARALLEL*)device)->Path);
		}

		free(device);
	}

	free(settings->DeviceArray);
	freerdp_settings_set_uint32(settings, FreeRDP_DeviceArraySize, 0);
	settings->DeviceArray = NULL;
	settings->DeviceCount = 0;
}

BOOL freerdp_static_channel_collection_add(rdpSettings* settings, ADDIN_ARGV* channel)
{
	UINT32 count;
	if (!settings->StaticChannelArray)
		return FALSE;

	if (freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelArraySize) <
	    (freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelCount) + 1))
	{
		UINT32 new_size;
		ADDIN_ARGV** new_array;
		new_size = freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelArraySize) * 2;
		new_array =
		    (ADDIN_ARGV**)realloc(settings->StaticChannelArray, new_size * sizeof(ADDIN_ARGV*));

		if (!new_array)
			return FALSE;

		settings->StaticChannelArray = new_array;
		if (!freerdp_settings_set_uint32(settings, FreeRDP_StaticChannelArraySize, new_size))
			return FALSE;
	}

	count = freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelCount);
	settings->StaticChannelArray[count++] = channel;
	return freerdp_settings_set_uint32(settings, FreeRDP_StaticChannelCount, count);
}

ADDIN_ARGV* freerdp_static_channel_collection_find(rdpSettings* settings, const char* name)
{
	UINT32 index;
	ADDIN_ARGV* channel;

	for (index = 0; index < freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelCount);
	     index++)
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
	_channel = (ADDIN_ARGV*)malloc(sizeof(ADDIN_ARGV));

	if (!_channel)
		return NULL;

	_channel->argc = channel->argc;
	_channel->argv = (char**)calloc(channel->argc, sizeof(char*));

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

	for (i = 0; i < freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelCount); i++)
	{
		if (!settings->StaticChannelArray[i])
			continue;

		for (j = 0; j < settings->StaticChannelArray[i]->argc; j++)
			free(settings->StaticChannelArray[i]->argv[j]);

		free(settings->StaticChannelArray[i]->argv);
		free(settings->StaticChannelArray[i]);
	}

	free(settings->StaticChannelArray);
	freerdp_settings_set_uint32(settings, FreeRDP_StaticChannelArraySize, 0);
	settings->StaticChannelArray = NULL;
	freerdp_settings_set_uint32(settings, FreeRDP_StaticChannelCount, 0);
}

BOOL freerdp_dynamic_channel_collection_add(rdpSettings* settings, ADDIN_ARGV* channel)
{
	UINT32 count;
	if (!settings->DynamicChannelArray)
		return FALSE;

	if (freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelArraySize) <
	    (freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelCount) + 1))
	{
		ADDIN_ARGV** new_array;
		const size_t size =
		    freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelArraySize) * 2;
		new_array = realloc(settings->DynamicChannelArray, sizeof(ADDIN_ARGV*) * size);

		if (!new_array)
			return FALSE;

		settings->DynamicChannelArray = new_array;
		if (!freerdp_settings_set_uint32(settings, FreeRDP_DynamicChannelArraySize, size))
			return FALSE;
	}

	count = freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelCount);
	settings->DynamicChannelArray[count++] = channel;
	return freerdp_settings_set_uint32(settings, FreeRDP_DynamicChannelCount, count);
}

ADDIN_ARGV* freerdp_dynamic_channel_collection_find(rdpSettings* settings, const char* name)
{
	UINT32 index;
	ADDIN_ARGV* channel;

	for (index = 0; index < freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelCount);
	     index++)
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
	_channel = (ADDIN_ARGV*)malloc(sizeof(ADDIN_ARGV));

	if (!_channel)
		return NULL;

	_channel->argc = channel->argc;
	_channel->argv = (char**)calloc(sizeof(char*), channel->argc);

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

	for (i = 0; i < freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelCount); i++)
	{
		if (!settings->DynamicChannelArray[i])
			continue;

		for (j = 0; j < settings->DynamicChannelArray[i]->argc; j++)
			free(settings->DynamicChannelArray[i]->argv[j]);

		free(settings->DynamicChannelArray[i]->argv);
		free(settings->DynamicChannelArray[i]);
	}

	free(settings->DynamicChannelArray);
	freerdp_settings_set_uint32(settings, FreeRDP_DynamicChannelArraySize, 0);
	settings->DynamicChannelArray = NULL;
	freerdp_settings_set_uint32(settings, FreeRDP_DynamicChannelCount, 0);
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
	UINT32 PerformanceFlags = PERF_FLAG_NONE;

	if (freerdp_settings_get_bool(settings, FreeRDP_AllowFontSmoothing))
		PerformanceFlags |= PERF_ENABLE_FONT_SMOOTHING;

	if (freerdp_settings_get_bool(settings, FreeRDP_AllowDesktopComposition))
		PerformanceFlags |= PERF_ENABLE_DESKTOP_COMPOSITION;

	if (freerdp_settings_get_bool(settings, FreeRDP_DisableWallpaper))
		PerformanceFlags |= PERF_DISABLE_WALLPAPER;

	if (freerdp_settings_get_bool(settings, FreeRDP_DisableFullWindowDrag))
		PerformanceFlags |= PERF_DISABLE_FULLWINDOWDRAG;

	if (freerdp_settings_get_bool(settings, FreeRDP_DisableMenuAnims))
		PerformanceFlags |= PERF_DISABLE_MENUANIMATIONS;

	if (freerdp_settings_get_bool(settings, FreeRDP_DisableThemes))
		PerformanceFlags |= PERF_DISABLE_THEMING;
	freerdp_settings_set_uint32(settings, FreeRDP_PerformanceFlags, PerformanceFlags);
}

void freerdp_performance_flags_split(rdpSettings* settings)
{
	freerdp_settings_set_bool(settings, FreeRDP_AllowFontSmoothing,
	                          (freerdp_settings_get_uint32(settings, FreeRDP_PerformanceFlags) &
	                           PERF_ENABLE_FONT_SMOOTHING)
	                              ? TRUE
	                              : FALSE);
	freerdp_settings_set_bool(settings, FreeRDP_AllowDesktopComposition,
	                          (freerdp_settings_get_uint32(settings, FreeRDP_PerformanceFlags) &
	                           PERF_ENABLE_DESKTOP_COMPOSITION)
	                              ? TRUE
	                              : FALSE);
	freerdp_settings_set_bool(
	    settings, FreeRDP_DisableWallpaper,
	    (freerdp_settings_get_uint32(settings, FreeRDP_PerformanceFlags) & PERF_DISABLE_WALLPAPER)
	        ? TRUE
	        : FALSE);
	freerdp_settings_set_bool(settings, FreeRDP_DisableFullWindowDrag,
	                          (freerdp_settings_get_uint32(settings, FreeRDP_PerformanceFlags) &
	                           PERF_DISABLE_FULLWINDOWDRAG)
	                              ? TRUE
	                              : FALSE);
	freerdp_settings_set_bool(settings, FreeRDP_DisableMenuAnims,
	                          (freerdp_settings_get_uint32(settings, FreeRDP_PerformanceFlags) &
	                           PERF_DISABLE_MENUANIMATIONS)
	                              ? TRUE
	                              : FALSE);
	freerdp_settings_set_bool(
	    settings, FreeRDP_DisableThemes,
	    (freerdp_settings_get_uint32(settings, FreeRDP_PerformanceFlags) & PERF_DISABLE_THEMING)
	        ? TRUE
	        : FALSE);
}

BOOL freerdp_set_gateway_usage_method(rdpSettings* settings, UINT32 GatewayUsageMethod)
{
	if (!freerdp_settings_set_uint32(settings, FreeRDP_GatewayUsageMethod, GatewayUsageMethod))
		return FALSE;

	if (GatewayUsageMethod == TSC_PROXY_MODE_NONE_DIRECT)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayEnabled, FALSE) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_GatewayBypassLocal, FALSE))
			return FALSE;
	}
	else if (GatewayUsageMethod == TSC_PROXY_MODE_DIRECT)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayEnabled, TRUE) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_GatewayBypassLocal, FALSE))
			return FALSE;
	}
	else if (GatewayUsageMethod == TSC_PROXY_MODE_DETECT)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayEnabled, TRUE) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_GatewayBypassLocal, TRUE))
			return FALSE;
	}
	else if (GatewayUsageMethod == TSC_PROXY_MODE_DEFAULT)
	{
		/**
		 * This corresponds to "Automatically detect RD Gateway server settings",
		 * which means the client attempts to use gateway group policy settings
		 * http://technet.microsoft.com/en-us/library/cc770601.aspx
		 */
		if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayEnabled, FALSE) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_GatewayBypassLocal, FALSE))
			return FALSE;
	}
	else if (GatewayUsageMethod == TSC_PROXY_MODE_NONE_DETECT)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayEnabled, FALSE) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_GatewayBypassLocal, FALSE))
			return FALSE;
	}

	return TRUE;
}

void freerdp_update_gateway_usage_method(rdpSettings* settings, UINT32 GatewayEnabled,
                                         UINT32 GatewayBypassLocal)
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

BOOL freerdp_get_param_bool(const rdpSettings* settings, int id)
{
	return freerdp_settings_get_bool(settings, (size_t)id);
}

int freerdp_set_param_bool(rdpSettings* settings, int id, BOOL param)
{
	return freerdp_settings_set_bool(settings, (size_t)id, param) ? 0 : -1;
}

int freerdp_get_param_int(const rdpSettings* settings, int id)
{
	return freerdp_settings_get_int32(settings, (size_t)id);
}

int freerdp_set_param_int(rdpSettings* settings, int id, int param)
{
	return freerdp_settings_set_int32(settings, (size_t)id, param) ? 0 : -1;
}

UINT32 freerdp_get_param_uint32(const rdpSettings* settings, int id)
{
	return freerdp_settings_get_uint32(settings, (size_t)id);
}

int freerdp_set_param_uint32(rdpSettings* settings, int id, UINT32 param)
{
	return freerdp_settings_set_uint32(settings, (size_t)id, param) ? 0 : -1;
}

UINT64 freerdp_get_param_uint64(const rdpSettings* settings, int id)
{
	return freerdp_settings_get_uint64(settings, (size_t)id);
}

int freerdp_set_param_uint64(rdpSettings* settings, int id, UINT64 param)
{
	return freerdp_settings_set_uint64(settings, (size_t)id, param) ? 0 : -1;
}

char* freerdp_get_param_string(const rdpSettings* settings, int id)
{
	return (char*)freerdp_settings_get_string(settings, (size_t)id);
}

int freerdp_set_param_string(rdpSettings* settings, int id, const char* param)
{
	return freerdp_settings_set_string(settings, (size_t)id, param) ? 0 : -1;
}

static BOOL value_to_uint(const char* value, ULONGLONG* result, ULONGLONG min, ULONGLONG max)
{
	unsigned long long rc;

	if (!value || !result)
		return FALSE;

	errno = 0;
	rc = _strtoui64(value, NULL, 0);

	if (errno != 0)
		return FALSE;

	if ((rc < min) || (rc > max))
		return FALSE;

	*result = rc;
	return TRUE;
}

static BOOL value_to_int(const char* value, LONGLONG* result, LONGLONG min, LONGLONG max)
{
	long long rc;

	if (!value || !result)
		return FALSE;

	errno = 0;
	rc = _strtoi64(value, NULL, 0);

	if (errno != 0)
		return FALSE;

	if ((rc < min) || (rc > max))
		return FALSE;

	*result = rc;
	return TRUE;
}

static BOOL parsing_fail(const char* key, const char* type, const char* value)
{
	WLog_ERR(TAG, "Failed to parse key [%s] of type [%s]: value [%s]", key, type, value);
	return FALSE;
}

BOOL freerdp_settings_set_value_for_name(rdpSettings* settings, const char* name, const char* value)
{
	ULONGLONG uval;
	LONGLONG ival;
	SSIZE_T index, type;
	if (!settings || !name)
		return FALSE;

	index = freerdp_settings_get_key_for_name(name);
	if (index < 0)
	{
		WLog_ERR(TAG, "Invalid settings key [%s]", name);
		return FALSE;
	}

	type = freerdp_settings_get_type_for_key((size_t)index);
	switch (type)
	{

		case RDP_SETTINGS_TYPE_BOOL:
		{
			BOOL val = _strnicmp(value, "TRUE", 5) == 0;
			if (!val && _strnicmp(value, "FALSE", 5) != 0)
				return parsing_fail(name, "BOOL", value);
			return freerdp_settings_set_bool(settings, index, val);
		}
		case RDP_SETTINGS_TYPE_UINT16:
			if (!value_to_uint(value, &uval, 0, UINT16_MAX))
				return parsing_fail(name, "UINT16", value);
			if (!freerdp_settings_set_uint16(settings, index, uval))
				return parsing_fail(name, "UINT16", value);
			return TRUE;

		case RDP_SETTINGS_TYPE_INT16:
			if (!value_to_int(value, &ival, INT16_MIN, INT16_MAX))
				return parsing_fail(name, "INT16", value);
			if (!freerdp_settings_set_int16(settings, index, ival))
				return parsing_fail(name, "INT16", value);
			return TRUE;
		case RDP_SETTINGS_TYPE_UINT32:
			if (!value_to_uint(value, &uval, 0, UINT32_MAX))
				return parsing_fail(name, "UINT32", value);
			if (!freerdp_settings_set_uint32(settings, index, uval))
				return parsing_fail(name, "UINT32", value);
			return TRUE;
		case RDP_SETTINGS_TYPE_INT32:
			if (!value_to_int(value, &ival, INT32_MIN, INT32_MAX))
				return parsing_fail(name, "INT32", value);
			if (!freerdp_settings_set_int32(settings, index, ival))
				return parsing_fail(name, "INT32", value);
			return TRUE;
		case RDP_SETTINGS_TYPE_UINT64:
			if (!value_to_uint(value, &uval, 0, UINT64_MAX))
				return parsing_fail(name, "UINT64", value);
			if (!freerdp_settings_set_uint64(settings, index, uval))
				return parsing_fail(name, "UINT64", value);
			return TRUE;
		case RDP_SETTINGS_TYPE_INT64:
			if (!value_to_int(value, &ival, INT64_MIN, INT64_MAX))
				return parsing_fail(name, "INT64", value);
			if (!freerdp_settings_set_int64(settings, index, ival))
				return parsing_fail(name, "INT64", value);
			return TRUE;

		case RDP_SETTINGS_TYPE_STRING:
			return freerdp_settings_set_string(settings, index, value);
		case RDP_SETTINGS_TYPE_POINTER:
			return parsing_fail(name, "POINTER", value);
		default:
			return FALSE;
	}
	return FALSE;
}

static BOOL freerdp_settings_set_pointer_len_(rdpSettings* settings, size_t id, SSIZE_T lenId,
                                              const void* data, size_t len)
{
	BOOL rc;
	void* copy;
	void* old = freerdp_settings_get_pointer_writable(settings, id);
	free(old);
	if (!freerdp_settings_set_pointer(settings, id, NULL))
		return FALSE;
	if (lenId >= 0)
	{
		if (!freerdp_settings_set_uint32(settings, lenId, 0))
			return FALSE;
	}

	if (len == 0)
		return TRUE;
	copy = calloc(len, 1);
	if (!copy)
		return FALSE;
	if (data)
		memcpy(copy, data, len);
	rc = freerdp_settings_set_pointer(settings, id, copy);
	if (!rc)
	{
		free(copy);
		return FALSE;
	}
	if (lenId < 0)
		return TRUE;
	return freerdp_settings_set_uint32(settings, lenId, len);
}

const void* freerdp_settings_get_pointer(const rdpSettings* settings, size_t id)
{
	return freerdp_settings_get_pointer_writable(settings, id);
}

BOOL freerdp_settings_set_pointer_len(rdpSettings* settings, size_t id, const void* data,
                                      size_t len)
{
	if (!settings)
		return FALSE;

	switch (id)
	{
		case FreeRDP_RdpServerCertificate:
			certificate_free(settings->RdpServerCertificate);
			settings->RdpServerCertificate = data;
			return TRUE;
		case FreeRDP_RdpServerRsaKey:
			key_free(settings->RdpServerRsaKey);
			settings->RdpServerRsaKey = (rdpRsaKey*)data;
			return TRUE;
		case FreeRDP_RedirectionPassword:
			return freerdp_settings_set_pointer_len_(settings, id,
			                                         FreeRDP_RedirectionPasswordLength, data, len);
		case FreeRDP_RedirectionTsvUrl:
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_RedirectionTsvUrlLength,
			                                         data, len);
		case FreeRDP_LoadBalanceInfo:
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_LoadBalanceInfoLength,
			                                         data, len);
		case FreeRDP_ServerRandom:
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_ServerRandomLength, data,
			                                         len);
		case FreeRDP_ClientRandom:
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_ClientRandomLength, data,
			                                         len);
		case FreeRDP_ServerCertificate:
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_ServerCertificateLength,
			                                         data, len);
		case FreeRDP_TargetNetAddresses:
			if (data == NULL)
			{
				freerdp_target_net_addresses_free(settings);
				if (!freerdp_settings_set_uint32(settings, FreeRDP_TargetNetAddressCount, len))
					return FALSE;
			}
			return freerdp_settings_set_pointer_len_(settings, FreeRDP_TargetNetAddresses,
			                                         FreeRDP_TargetNetAddressCount, data, len);

		case FreeRDP_TargetNetPorts:
			if (data == NULL)
			{
				freerdp_target_net_addresses_free(settings);
				if (!freerdp_settings_set_uint32(settings, FreeRDP_TargetNetAddressCount, len))
					return FALSE;
			}
			return freerdp_settings_set_pointer_len_(settings, FreeRDP_TargetNetPorts,
			                                         FreeRDP_TargetNetAddressCount, data, len);
		case FreeRDP_ClientAutoReconnectCookie:
		case FreeRDP_ServerAutoReconnectCookie:
		case FreeRDP_ChannelDefArray:
		case FreeRDP_MonitorDefArray:
		case FreeRDP_MonitorIds:
		case FreeRDP_ReceivedCapabilities:
		case FreeRDP_OrderSupport:
		case FreeRDP_ClientTimeZone:
		case FreeRDP_BitmapCacheV2CellInfo:
		case FreeRDP_GlyphCache:
		case FreeRDP_FragCache:
			return freerdp_settings_set_pointer_len_(settings, id, -1, data, len);
		default:
			if ((data == NULL) && (len == 0))
			{
				freerdp_settings_set_pointer(settings, id, NULL);
			}
			else
				WLog_WARN(TAG, "Invalid id %" PRIuz " for %s", id, __FUNCTION__);
			return FALSE;
	}
}

void* freerdp_settings_get_pointer_array_writable(const rdpSettings* settings, size_t id,
                                                  size_t offset)
{
	if (!settings)
		return NULL;
	switch (id)
	{
		case FreeRDP_OrderSupport:
			if (offset >= 32)
				return FALSE;
			return &settings->OrderSupport[offset];
		case FreeRDP_MonitorIds:
			if (offset > freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorIds))
				return NULL;
			return &settings->MonitorIds[offset];
		case FreeRDP_MonitorDefArray:
			if (offset > freerdp_settings_get_uint32(settings, FreeRDP_MonitorDefArraySize))
				return NULL;
			return &settings->MonitorDefArray[offset];

		case FreeRDP_ChannelDefArray:
			if (offset > freerdp_settings_get_uint32(settings, FreeRDP_ChannelDefArraySize))
				return NULL;
			return &settings->ChannelDefArray[offset];
		case FreeRDP_DeviceArray:
			if (offset > freerdp_settings_get_uint32(settings, FreeRDP_DeviceArraySize))
				return NULL;
			return &settings->DeviceArray[offset];
		case FreeRDP_StaticChannelArray:
			if (offset > freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelArraySize))
				return NULL;
			return settings->StaticChannelArray[offset];
		case FreeRDP_DynamicChannelArray:
			if (offset > freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelArraySize))
				return NULL;
			return settings->DynamicChannelArray[offset];
		case FreeRDP_FragCache:
			if (offset >= 1)
				return NULL;
			return &settings->FragCache[offset];
		case FreeRDP_GlyphCache:
			if (offset >= 10)
				return NULL;
			return &settings->GlyphCache[offset];
		case FreeRDP_BitmapCacheV2CellInfo:
			/* TODO: BitmapCacheV2NumCells should be limited to 4
			if (offset > freerdp_settings_get_uint32(settings, FreeRDP_BitmapCacheV2NumCells))
			    return NULL;
			    */

			return &settings->BitmapCacheV2CellInfo[offset];
		case FreeRDP_ReceivedCapabilities:
			if (offset > settings->ReceivedCapabilitiesSize)
				return 0;
			return &settings->ReceivedCapabilities[offset];
		default:
			WLog_WARN(TAG, "Invalid id %" PRIuz " for %s", id, __FUNCTION__);
			return NULL;
	}
}

BOOL freerdp_settings_set_pointer_array(rdpSettings* settings, size_t id, size_t offset,
                                        const void* data)
{
	if (!settings)
		return FALSE;
	switch (id)
	{
		case FreeRDP_TargetNetAddresses:
			if ((offset >= settings->TargetNetAddressCount) || !data)
				return FALSE;
			free(settings->TargetNetAddresses[offset]);
			settings->TargetNetAddresses[offset] = _strdup((const char*)data);
			return settings->TargetNetAddresses[offset] != NULL;
		case FreeRDP_TargetNetPorts:
			if ((offset >= settings->TargetNetAddressCount) || !data)
				return FALSE;
			settings->TargetNetPorts[offset] = *((const UINT32*)data);
			return TRUE;
		case FreeRDP_BitmapCacheV2CellInfo:
			if ((offset > 5) || !data)
				return FALSE;
			settings->BitmapCacheV2CellInfo[offset] = *(const BITMAP_CACHE_V2_CELL_INFO*)data;
			return TRUE;
		case FreeRDP_OrderSupport:
			if ((offset >= 32) || !data)
				return FALSE;
			settings->OrderSupport[offset] = *(const BOOL*)data;
			return TRUE;
		case FreeRDP_GlyphCache:
			if ((offset >= 10) || !data)
				return FALSE;
			settings->GlyphCache[offset] = *(const GLYPH_CACHE_DEFINITION*)data;
			return TRUE;
		case FreeRDP_FragCache:
			if ((offset >= 1) || !data)
				return FALSE;
			settings->FragCache[offset] = *(const GLYPH_CACHE_DEFINITION*)data;
			return TRUE;
		case FreeRDP_MonitorIds:
			if ((offset > freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorIds)) || !data)
				return FALSE;
			settings->MonitorIds[offset] = *(const UINT32*)data;
			return TRUE;
		case FreeRDP_ChannelDefArray:
			if (offset > freerdp_settings_get_uint32(settings, FreeRDP_ChannelDefArraySize))
				return FALSE;
			settings->ChannelDefArray[offset] = *(const CHANNEL_DEF*)data;
			return TRUE;
		default:
			WLog_WARN(TAG, "Invalid id %" PRIuz " for %s", id, __FUNCTION__);
			return FALSE;
	}
}

const void* freerdp_settings_get_pointer_array(const rdpSettings* settings, size_t id,
                                               size_t offset)
{
	return freerdp_settings_get_pointer_array_writable(settings, id, offset);
}

UINT32 freerdp_settings_get_codecs_flags(const rdpSettings* settings)
{
	UINT32 flags = FREERDP_CODEC_ALL;
	if (settings->RemoteFxCodec == FALSE)
	{
		flags &= ~FREERDP_CODEC_REMOTEFX;
	}
	if (settings->NSCodec == FALSE)
	{
		flags &= ~FREERDP_CODEC_NSCODEC;
	}
	/*TODO: check other codecs flags */
	return flags;
}
