/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Settings Management
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016 Armin Novak <armin.novak@gmail.com>
 * Copyright 2023 Armin Novak <anovak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/cast.h>

#include "../core/settings.h"
#include "../core/capabilities.h"

#include <freerdp/crypto/certificate.h>
#include <freerdp/settings.h>
#include <freerdp/freerdp.h>
#include <freerdp/log.h>

#define TAG FREERDP_TAG("common")

BOOL freerdp_addin_argv_add_argument_ex(ADDIN_ARGV* args, const char* argument, size_t len)
{
	if (!args || !argument)
		return FALSE;

	if (len == 0)
		len = strlen(argument);

	char** new_argv = (char**)realloc(
	    (void*)args->argv, sizeof(char*) * (WINPR_ASSERTING_INT_CAST(uint32_t, args->argc) + 1));

	if (!new_argv)
		return FALSE;

	args->argv = new_argv;

	char* str = calloc(len + 1, sizeof(char));
	if (!str)
		return FALSE;
	memcpy(str, argument, len);
	args->argv[args->argc++] = str;
	return TRUE;
}

BOOL freerdp_addin_argv_add_argument(ADDIN_ARGV* args, const char* argument)
{
	return freerdp_addin_argv_add_argument_ex(args, argument, 0);
}

BOOL freerdp_addin_argv_del_argument(ADDIN_ARGV* args, const char* argument)
{
	if (!args || !argument)
		return FALSE;
	for (int x = 0; x < args->argc; x++)
	{
		char* arg = args->argv[x];
		if (strcmp(argument, arg) == 0)
		{
			free(arg);
			memmove_s((void*)&args->argv[x],
			          (WINPR_ASSERTING_INT_CAST(uint32_t, args->argc - x)) * sizeof(char*),
			          (void*)&args->argv[x + 1],
			          (WINPR_ASSERTING_INT_CAST(uint32_t, args->argc - x - 1)) * sizeof(char*));
			args->argv[args->argc - 1] = NULL;
			args->argc--;
			return TRUE;
		}
	}
	return FALSE;
}

int freerdp_addin_set_argument(ADDIN_ARGV* args, const char* argument)
{
	if (!args || !argument)
		return -2;

	for (int i = 0; i < args->argc; i++)
	{
		if (strcmp(args->argv[i], argument) == 0)
		{
			return 1;
		}
	}

	if (!freerdp_addin_argv_add_argument(args, argument))
		return -1;
	return 0;
}

int freerdp_addin_replace_argument(ADDIN_ARGV* args, const char* previous, const char* argument)
{
	if (!args || !previous || !argument)
		return -2;

	for (int i = 0; i < args->argc; i++)
	{
		if (strcmp(args->argv[i], previous) == 0)
		{
			free(args->argv[i]);

			if (!(args->argv[i] = _strdup(argument)))
				return -1;

			return 1;
		}
	}

	if (!freerdp_addin_argv_add_argument(args, argument))
		return -1;
	return 0;
}

int freerdp_addin_set_argument_value(ADDIN_ARGV* args, const char* option, const char* value)
{
	BOOL rc = 0;
	char* p = NULL;
	char* str = NULL;
	size_t length = 0;
	if (!args || !option || !value)
		return -2;
	length = strlen(option) + strlen(value) + 1;
	str = (char*)calloc(length + 1, sizeof(char));

	if (!str)
		return -1;

	(void)sprintf_s(str, length + 1, "%s:%s", option, value);

	for (int i = 0; i < args->argc; i++)
	{
		p = strchr(args->argv[i], ':');

		if (p)
		{
			if (strncmp(args->argv[i], option,
			            WINPR_ASSERTING_INT_CAST(size_t, p - args->argv[i])) == 0)
			{
				free(args->argv[i]);
				args->argv[i] = str;
				return 1;
			}
		}
	}

	rc = freerdp_addin_argv_add_argument(args, str);
	free(str);
	if (!rc)
		return -1;
	return 0;
}

int freerdp_addin_replace_argument_value(ADDIN_ARGV* args, const char* previous, const char* option,
                                         const char* value)
{
	BOOL rc = 0;
	char* str = NULL;
	size_t length = 0;
	if (!args || !previous || !option || !value)
		return -2;
	length = strlen(option) + strlen(value) + 1;
	str = (char*)calloc(length + 1, sizeof(char));

	if (!str)
		return -1;

	(void)sprintf_s(str, length + 1, "%s:%s", option, value);

	for (int i = 0; i < args->argc; i++)
	{
		if (strcmp(args->argv[i], previous) == 0)
		{
			free(args->argv[i]);
			args->argv[i] = str;
			return 1;
		}
	}

	rc = freerdp_addin_argv_add_argument(args, str);
	free(str);
	if (!rc)
		return -1;
	return 0;
}

BOOL freerdp_device_collection_add(rdpSettings* settings, RDPDR_DEVICE* device)
{
	UINT32 count = 0;
	UINT32 old = 0;
	WINPR_ASSERT(settings);
	WINPR_ASSERT(device);

	count = freerdp_settings_get_uint32(settings, FreeRDP_DeviceCount) + 1;
	old = freerdp_settings_get_uint32(settings, FreeRDP_DeviceArraySize);
	if (old < count)
	{
		const size_t new_size = (old + 32);
		RDPDR_DEVICE** new_array =
		    (RDPDR_DEVICE**)realloc((void*)settings->DeviceArray, new_size * sizeof(RDPDR_DEVICE*));

		if (!new_array)
			return FALSE;

		settings->DeviceArray = new_array;
		for (size_t x = old; x < new_size; x++)
			settings->DeviceArray[x] = NULL;

		if (!freerdp_settings_set_uint32(settings, FreeRDP_DeviceArraySize,
		                                 WINPR_ASSERTING_INT_CAST(uint32_t, new_size)))
			return FALSE;
	}

	settings->DeviceArray[settings->DeviceCount++] = device;
	return TRUE;
}

BOOL freerdp_device_collection_del(rdpSettings* settings, const RDPDR_DEVICE* device)
{
	WINPR_ASSERT(settings);

	if (!device)
		return FALSE;

	const UINT32 count = settings->DeviceCount;
	for (size_t x = 0; x < count; x++)
	{
		const RDPDR_DEVICE* cur = settings->DeviceArray[x];
		if (cur == device)
		{
			for (size_t y = x + 1; y < count; y++)
			{
				RDPDR_DEVICE* next = settings->DeviceArray[y];
				settings->DeviceArray[y - 1] = next;
			}
			settings->DeviceArray[count - 1] = NULL;
			settings->DeviceCount--;
			return TRUE;
		}
	}

	return FALSE;
}

RDPDR_DEVICE* freerdp_device_collection_find(rdpSettings* settings, const char* name)
{
	RDPDR_DEVICE* device = NULL;

	WINPR_ASSERT(settings);
	WINPR_ASSERT(name);
	for (UINT32 index = 0; index < settings->DeviceCount; index++)
	{
		device = settings->DeviceArray[index];

		if (!device->Name)
			continue;

		if (strcmp(device->Name, name) == 0)
			return device;
	}

	return NULL;
}

RDPDR_DEVICE* freerdp_device_collection_find_type(rdpSettings* settings, UINT32 type)
{
	RDPDR_DEVICE* device = NULL;
	WINPR_ASSERT(settings);

	for (UINT32 index = 0; index < settings->DeviceCount; index++)
	{
		device = settings->DeviceArray[index];

		if (device->Type == type)
			return device;
	}

	return NULL;
}

RDPDR_DEVICE* freerdp_device_new(UINT32 Type, size_t count, const char* const args[])
{
	size_t size = 0;
	union
	{
		RDPDR_DEVICE* base;
		RDPDR_DRIVE* drive;
		RDPDR_SERIAL* serial;
		RDPDR_PRINTER* printer;
		RDPDR_PARALLEL* parallel;
		RDPDR_SMARTCARD* smartcard;
	} device;

	device.base = NULL;
	WINPR_ASSERT(args || (count == 0));

	switch (Type)
	{
		case RDPDR_DTYP_PRINT:
			size = sizeof(RDPDR_PRINTER);
			break;
		case RDPDR_DTYP_SERIAL:
			size = sizeof(RDPDR_SERIAL);
			break;
		case RDPDR_DTYP_PARALLEL:
			size = sizeof(RDPDR_PARALLEL);
			break;
		case RDPDR_DTYP_SMARTCARD:
			size = sizeof(RDPDR_SMARTCARD);
			break;
		case RDPDR_DTYP_FILESYSTEM:
			size = sizeof(RDPDR_DRIVE);
			break;
		default:
			goto fail;
	}

	device.base = calloc(1, size);
	if (!device.base)
		goto fail;
	device.base->Id = 0;
	device.base->Type = Type;

	if (count > 0)
	{
		device.base->Name = _strdup(args[0]);
		if (!device.base->Name)
			goto fail;

		switch (Type)
		{
			case RDPDR_DTYP_PRINT:
				if (count > 1)
				{
					device.printer->DriverName = _strdup(args[1]);
					if (!device.printer->DriverName)
						goto fail;
				}

				if (count > 2)
				{
					device.printer->IsDefault = _stricmp(args[2], "default") == 0;
				}
				break;
			case RDPDR_DTYP_SERIAL:
				if (count > 1)
				{
					device.serial->Path = _strdup(args[1]);
					if (!device.serial->Path)
						goto fail;
				}

				if (count > 2)
				{
					device.serial->Driver = _strdup(args[2]);
					if (!device.serial->Driver)
						goto fail;
				}

				if (count > 3)
				{
					device.serial->Permissive = _strdup(args[3]);
					if (!device.serial->Permissive)
						goto fail;
				}
				break;
			case RDPDR_DTYP_PARALLEL:
				if (count > 1)
				{
					device.parallel->Path = _strdup(args[1]);
					if (!device.serial->Path)
						goto fail;
				}
				break;
			case RDPDR_DTYP_SMARTCARD:
				break;
			case RDPDR_DTYP_FILESYSTEM:
				if (count > 1)
				{
					device.drive->Path = _strdup(args[1]);
					if (!device.drive->Path)
						goto fail;
				}
				if (count > 2)
					device.drive->automount = (args[2] == NULL) ? TRUE : FALSE;
				break;
			default:
				goto fail;
		}
	}
	return device.base;

fail:
	freerdp_device_free(device.base);
	return NULL;
}

void freerdp_device_free(RDPDR_DEVICE* device)
{
	if (!device)
		return;

	union
	{
		RDPDR_DEVICE* dev;
		RDPDR_DRIVE* drive;
		RDPDR_SERIAL* serial;
		RDPDR_PRINTER* printer;
		RDPDR_PARALLEL* parallel;
		RDPDR_SMARTCARD* smartcard;
	} cnv;

	cnv.dev = device;

	switch (device->Type)
	{
		case RDPDR_DTYP_PRINT:
			free(cnv.printer->DriverName);
			break;
		case RDPDR_DTYP_SERIAL:
			free(cnv.serial->Path);
			free(cnv.serial->Driver);
			free(cnv.serial->Permissive);
			break;
		case RDPDR_DTYP_PARALLEL:
			free(cnv.parallel->Path);
			break;
		case RDPDR_DTYP_SMARTCARD:
			break;
		case RDPDR_DTYP_FILESYSTEM:
			free(cnv.drive->Path);
			break;
		default:
			break;
	}
	free(cnv.dev->Name);
	free(cnv.dev);
}

RDPDR_DEVICE* freerdp_device_clone(const RDPDR_DEVICE* device)
{
	union
	{
		const RDPDR_DEVICE* dev;
		const RDPDR_DRIVE* drive;
		const RDPDR_SERIAL* serial;
		const RDPDR_PRINTER* printer;
		const RDPDR_PARALLEL* parallel;
		const RDPDR_SMARTCARD* smartcard;
	} src;

	union
	{
		RDPDR_DEVICE* dev;
		RDPDR_DRIVE* drive;
		RDPDR_SERIAL* serial;
		RDPDR_PRINTER* printer;
		RDPDR_PARALLEL* parallel;
		RDPDR_SMARTCARD* smartcard;
	} copy;
	size_t count = 0;
	const char* args[4] = { 0 };

	copy.dev = NULL;
	src.dev = device;

	if (!device)
		return NULL;

	if (device->Name)
	{
		args[count++] = device->Name;
	}

	switch (device->Type)
	{
		case RDPDR_DTYP_FILESYSTEM:
			if (src.drive->Path)
			{
				args[count++] = src.drive->Path;
				args[count++] = src.drive->automount ? NULL : src.drive->Path;
			}
			break;

		case RDPDR_DTYP_PRINT:
			if (src.printer->DriverName)
				args[count++] = src.printer->DriverName;
			break;

		case RDPDR_DTYP_SMARTCARD:
			break;

		case RDPDR_DTYP_SERIAL:
			if (src.serial->Path)
				args[count++] = src.serial->Path;

			if (src.serial->Driver)
				args[count++] = src.serial->Driver;

			if (src.serial->Permissive)
				args[count++] = src.serial->Permissive;
			break;

		case RDPDR_DTYP_PARALLEL:
			if (src.parallel->Path)
				args[count++] = src.parallel->Path;
			break;
		default:
			WLog_ERR(TAG, "unknown device type %" PRIu32 "", device->Type);
			break;
	}

	copy.dev = freerdp_device_new(device->Type, count, args);
	if (!copy.dev)
		return NULL;

	copy.dev->Id = device->Id;

	return copy.dev;
}

void freerdp_device_collection_free(rdpSettings* settings)
{
	WINPR_ASSERT(settings);

	if (settings->DeviceArray)
	{
		for (UINT32 index = 0; index < settings->DeviceArraySize; index++)
			(void)freerdp_settings_set_pointer_array(settings, FreeRDP_DeviceArray, index, NULL);
	}

	free((void*)settings->DeviceArray);

	(void)freerdp_settings_set_pointer(settings, FreeRDP_DeviceArray, NULL);
	(void)freerdp_settings_set_uint32(settings, FreeRDP_DeviceArraySize, 0);
	(void)freerdp_settings_set_uint32(settings, FreeRDP_DeviceCount, 0);
}

BOOL freerdp_static_channel_collection_del(rdpSettings* settings, const char* name)
{
	const UINT32 count = freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelCount);
	if (!settings || !settings->StaticChannelArray)
		return FALSE;

	for (UINT32 x = 0; x < count; x++)
	{
		ADDIN_ARGV* cur = settings->StaticChannelArray[x];
		if (cur && (cur->argc > 0))
		{
			if (strcmp(name, cur->argv[0]) == 0)
			{
				memmove_s((void*)&settings->StaticChannelArray[x],
				          (count - x) * sizeof(ADDIN_ARGV*),
				          (void*)&settings->StaticChannelArray[x + 1],
				          (count - x - 1) * sizeof(ADDIN_ARGV*));
				for (size_t y = count - 1; y < settings->StaticChannelArraySize; y++)
					settings->StaticChannelArray[y] = NULL;

				freerdp_addin_argv_free(cur);
				return freerdp_settings_set_uint32(settings, FreeRDP_StaticChannelCount, count - 1);
			}
		}
	}
	{
		for (size_t x = count; x < settings->StaticChannelArraySize; x++)
			settings->StaticChannelArray[x] = NULL;
	}
	return FALSE;
}

BOOL freerdp_static_channel_collection_add(rdpSettings* settings, ADDIN_ARGV* channel)
{
	UINT32 count = 0;

	WINPR_ASSERT(settings);
	WINPR_ASSERT(channel);

	count = freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelCount) + 1;
	if (freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelArraySize) < count)
	{
		const UINT32 oldSize =
		    freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelArraySize);
		const size_t new_size = oldSize + 32ul;
		ADDIN_ARGV** new_array = (ADDIN_ARGV**)realloc((void*)settings->StaticChannelArray,
		                                               new_size * sizeof(ADDIN_ARGV*));

		if (!new_array)
			return FALSE;

		settings->StaticChannelArray = new_array;
		{
			for (size_t x = oldSize; x < new_size; x++)
				settings->StaticChannelArray[x] = NULL;
		}
		if (!freerdp_settings_set_uint32(settings, FreeRDP_StaticChannelArraySize,
		                                 WINPR_ASSERTING_INT_CAST(uint32_t, new_size)))
			return FALSE;
	}

	count = freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelCount);

	ADDIN_ARGV** cur = &settings->StaticChannelArray[count++];
	freerdp_addin_argv_free(*cur);
	*cur = channel;
	return freerdp_settings_set_uint32(settings, FreeRDP_StaticChannelCount, count);
}

ADDIN_ARGV* freerdp_static_channel_collection_find(rdpSettings* settings, const char* name)
{
	ADDIN_ARGV* channel = NULL;

	WINPR_ASSERT(settings);
	WINPR_ASSERT(name);

	for (UINT32 index = 0;
	     index < freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelCount); index++)
	{
		channel = settings->StaticChannelArray[index];

		if (strcmp(channel->argv[0], name) == 0)
			return channel;
	}

	return NULL;
}

void freerdp_static_channel_collection_free(rdpSettings* settings)
{
	if (!settings)
		return;

	if (settings->StaticChannelArray)
	{
		for (UINT32 i = 0;
		     i < freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelArraySize); i++)
			freerdp_addin_argv_free(settings->StaticChannelArray[i]);
	}

	free((void*)settings->StaticChannelArray);
	(void)freerdp_settings_set_uint32(settings, FreeRDP_StaticChannelArraySize, 0);
	settings->StaticChannelArray = NULL;
	(void)freerdp_settings_set_uint32(settings, FreeRDP_StaticChannelCount, 0);
}

BOOL freerdp_dynamic_channel_collection_del(rdpSettings* settings, const char* name)
{
	const UINT32 count = freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelCount);
	if (!settings || !settings->DynamicChannelArray)
		return FALSE;

	for (UINT32 x = 0; x < count; x++)
	{
		ADDIN_ARGV* cur = settings->DynamicChannelArray[x];
		if (cur && (cur->argc > 0))
		{
			if (strcmp(name, cur->argv[0]) == 0)
			{
				memmove_s((void*)&settings->DynamicChannelArray[x],
				          (count - x) * sizeof(ADDIN_ARGV*),
				          (void*)&settings->DynamicChannelArray[x + 1],
				          (count - x - 1) * sizeof(ADDIN_ARGV*));
				for (size_t y = count - 1; y < settings->DynamicChannelArraySize; y++)
					settings->DynamicChannelArray[y] = NULL;

				freerdp_addin_argv_free(cur);
				return freerdp_settings_set_uint32(settings, FreeRDP_DynamicChannelCount,
				                                   count - 1);
			}
		}
	}

	return FALSE;
}

BOOL freerdp_dynamic_channel_collection_add(rdpSettings* settings, ADDIN_ARGV* channel)
{
	UINT32 count = 0;
	UINT32 oldSize = 0;

	WINPR_ASSERT(settings);
	WINPR_ASSERT(channel);

	count = freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelCount) + 1;
	oldSize = freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelArraySize);
	if (oldSize < count)
	{

		const size_t size = oldSize + 32;
		ADDIN_ARGV** new_array =
		    (ADDIN_ARGV**)realloc((void*)settings->DynamicChannelArray, sizeof(ADDIN_ARGV*) * size);

		if (!new_array)
			return FALSE;

		settings->DynamicChannelArray = new_array;
		{
			for (size_t x = oldSize; x < size; x++)
				settings->DynamicChannelArray[x] = NULL;
		}
		if (!freerdp_settings_set_uint32(settings, FreeRDP_DynamicChannelArraySize,
		                                 WINPR_ASSERTING_INT_CAST(uint32_t, size)))
			return FALSE;
	}

	count = freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelCount);
	settings->DynamicChannelArray[count++] = channel;
	return freerdp_settings_set_uint32(settings, FreeRDP_DynamicChannelCount, count);
}

ADDIN_ARGV* freerdp_dynamic_channel_collection_find(const rdpSettings* settings, const char* name)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(name);

	for (UINT32 index = 0;
	     index < freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelCount); index++)
	{
		ADDIN_ARGV* channel = settings->DynamicChannelArray[index];

		if (strcmp(channel->argv[0], name) == 0)
			return channel;
	}

	return NULL;
}

void freerdp_addin_argv_free(ADDIN_ARGV* args)
{
	if (!args)
		return;

	if (args->argv)
	{
		for (int index = 0; index < args->argc; index++)
			free(args->argv[index]);
		free((void*)args->argv);
	}

	free(args);
}

ADDIN_ARGV* freerdp_addin_argv_new(size_t argc, const char* const argv[])
{
	if (argc > INT32_MAX)
		return NULL;

	ADDIN_ARGV* args = calloc(1, sizeof(ADDIN_ARGV));
	if (!args)
		return NULL;
	if (argc == 0)
		return args;

	args->argc = (int)argc;
	args->argv = (char**)calloc(argc, sizeof(char*));
	if (!args->argv)
		goto fail;

	if (argv)
	{
		for (size_t x = 0; x < argc; x++)
		{
			args->argv[x] = _strdup(argv[x]);
			if (!args->argv[x])
				goto fail;
		}
	}
	return args;

fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	freerdp_addin_argv_free(args);
	WINPR_PRAGMA_DIAG_POP
	return NULL;
}

ADDIN_ARGV* freerdp_addin_argv_clone(const ADDIN_ARGV* args)
{
	union
	{
		char** c;
		const char** cc;
	} cnv;
	if (!args)
		return NULL;
	cnv.c = args->argv;
	return freerdp_addin_argv_new(WINPR_ASSERTING_INT_CAST(uint32_t, args->argc), cnv.cc);
}

void freerdp_dynamic_channel_collection_free(rdpSettings* settings)
{
	WINPR_ASSERT(settings);

	if (settings->DynamicChannelArray)
	{
		for (UINT32 i = 0;
		     i < freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelArraySize); i++)
			freerdp_addin_argv_free(settings->DynamicChannelArray[i]);
	}

	free((void*)settings->DynamicChannelArray);
	(void)freerdp_settings_set_uint32(settings, FreeRDP_DynamicChannelArraySize, 0);
	settings->DynamicChannelArray = NULL;
	(void)freerdp_settings_set_uint32(settings, FreeRDP_DynamicChannelCount, 0);
}

static void freerdp_capability_data_free(rdpSettings* settings, size_t offset, BOOL full)
{
	WINPR_ASSERT(settings);

	if (settings->ReceivedCapabilityData)
	{
		for (size_t x = offset; x < settings->ReceivedCapabilitiesSize; x++)
		{
			free(settings->ReceivedCapabilityData[x]);
			settings->ReceivedCapabilityData[x] = NULL;
		}
		if (full)
		{
			free((void*)settings->ReceivedCapabilityData);
			settings->ReceivedCapabilityData = NULL;
		}
	}
}

void freerdp_capability_buffer_free(rdpSettings* settings)
{
	WINPR_ASSERT(settings);

	freerdp_capability_data_free(settings, 0, TRUE);

	free(settings->ReceivedCapabilityDataSizes);
	settings->ReceivedCapabilityDataSizes = NULL;

	free(settings->ReceivedCapabilities);
	settings->ReceivedCapabilities = NULL;

	settings->ReceivedCapabilitiesSize = 0;
}

static BOOL resize_setting(rdpSettings* settings, FreeRDP_Settings_Keys_Pointer id, size_t oldsize,
                           size_t size, size_t base)
{
	void* old = freerdp_settings_get_pointer_writable(settings, id);
	uint8_t* ptr = realloc(old, size * base);
	if (!ptr)
		return FALSE;

	if (size > oldsize)
	{
		const size_t diff = size - oldsize;
		memset(&ptr[oldsize * base], 0, diff * base);
	}

	// NOLINTNEXTLINE(clang-analyzer-unix.Malloc
	return freerdp_settings_set_pointer(settings, id, ptr);
}

static BOOL resize_setting_ptr(rdpSettings* settings, FreeRDP_Settings_Keys_Pointer id,
                               size_t oldsize, size_t size, size_t base)
{
	WINPR_ASSERT(base == sizeof(void*));

	uint8_t* old = freerdp_settings_get_pointer_writable(settings, id);
	if (size < oldsize)
	{
		uint8_t** optr = WINPR_REINTERPRET_CAST(old, uint8_t*, uint8_t**);
		for (size_t x = size; x < oldsize; x++)
		{
			uint8_t* ptr = optr[x];
			free(ptr);
		}
	}
	uint8_t* ptr = realloc(old, size * base);
	if (!ptr)
		return FALSE;

	uint8_t** optr = WINPR_REINTERPRET_CAST(ptr, uint8_t*, uint8_t**);
	for (size_t x = oldsize; x < size; x++)
	{
		optr[x] = NULL;
	}

	// NOLINTNEXTLINE(clang-analyzer-unix.Malloc
	return freerdp_settings_set_pointer(settings, id, ptr);
}

BOOL freerdp_capability_buffer_resize(rdpSettings* settings, size_t count, BOOL force)
{
	WINPR_ASSERT(settings);

	const uint32_t len = settings->ReceivedCapabilitiesSize;
	if (!force)
	{
		if (len == count)
			return TRUE;
	}

	freerdp_capability_data_free(settings, count, FALSE);

	if (count == 0)
	{
		freerdp_capability_buffer_free(settings);
		return TRUE;
	}

	const size_t oldsize = settings->ReceivedCapabilitiesSize;
	if (!resize_setting(settings, FreeRDP_ReceivedCapabilityDataSizes, oldsize, count,
	                    sizeof(uint32_t)))
		return FALSE;
	if (!resize_setting_ptr(settings, FreeRDP_ReceivedCapabilityData, oldsize, count,
	                        sizeof(uint8_t*)))
		return FALSE;
	if (!resize_setting(settings, FreeRDP_ReceivedCapabilities, oldsize, count, sizeof(uint32_t)))
		return FALSE;

	settings->ReceivedCapabilitiesSize = WINPR_ASSERTING_INT_CAST(uint32_t, count);
	return TRUE;
}

BOOL freerdp_capability_buffer_copy(rdpSettings* settings, const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	if (src->ReceivedCapabilitiesSize == 0)
		return TRUE;

	if (!freerdp_capability_buffer_resize(settings, src->ReceivedCapabilitiesSize, TRUE))
		return FALSE;

	for (UINT32 x = 0; x < src->ReceivedCapabilitiesSize; x++)
	{
		WINPR_ASSERT(settings->ReceivedCapabilities);
		settings->ReceivedCapabilities[x] = src->ReceivedCapabilities[x];

		WINPR_ASSERT(settings->ReceivedCapabilityDataSizes);
		settings->ReceivedCapabilityDataSizes[x] = src->ReceivedCapabilityDataSizes[x];

		WINPR_ASSERT(settings->ReceivedCapabilityData);
		if (src->ReceivedCapabilityDataSizes[x] > 0)
		{
			void* tmp = realloc(settings->ReceivedCapabilityData[x],
			                    settings->ReceivedCapabilityDataSizes[x]);
			if (!tmp)
				return FALSE;
			memcpy(tmp, src->ReceivedCapabilityData[x], src->ReceivedCapabilityDataSizes[x]);
			settings->ReceivedCapabilityData[x] = tmp;
		}
		else
		{
			free(settings->ReceivedCapabilityData[x]);
			settings->ReceivedCapabilityData[x] = NULL;
		}
	}
	return TRUE;
}

static void target_net_addresses_free(rdpSettings* settings, size_t offset)
{
	WINPR_ASSERT(settings);

	if (settings->TargetNetAddresses)
	{
		for (size_t index = offset; index < settings->TargetNetAddressCount; index++)
		{
			free(settings->TargetNetAddresses[index]);
			settings->TargetNetAddresses[index] = NULL;
		}
	}
}

void freerdp_target_net_addresses_free(rdpSettings* settings)
{
	WINPR_ASSERT(settings);

	target_net_addresses_free(settings, 0);

	free((void*)settings->TargetNetAddresses);
	settings->TargetNetAddresses = NULL;

	free(settings->TargetNetPorts);
	settings->TargetNetPorts = NULL;

	settings->TargetNetAddressCount = 0;
}

BOOL freerdp_target_net_addresses_resize(rdpSettings* settings, size_t count)
{
	WINPR_ASSERT(settings);

	if (count == 0)
	{
		freerdp_target_net_addresses_free(settings);
		return TRUE;
	}

	const uint32_t len = settings->TargetNetAddressCount;
	if (!resize_setting_ptr(settings, FreeRDP_TargetNetAddresses, len, count, sizeof(char*)))
		return FALSE;
	if (!resize_setting(settings, FreeRDP_TargetNetPorts, len, count, sizeof(uint32_t)))
		return FALSE;

	settings->TargetNetAddressCount = WINPR_ASSERTING_INT_CAST(uint32_t, count);
	return TRUE;
}

void freerdp_server_license_issuers_free(rdpSettings* settings)
{
	WINPR_ASSERT(settings);

	if (settings->ServerLicenseProductIssuers)
	{
		for (UINT32 x = 0; x < settings->ServerLicenseProductIssuersCount; x++)
			free(settings->ServerLicenseProductIssuers[x]);
	}
	free((void*)settings->ServerLicenseProductIssuers);
	settings->ServerLicenseProductIssuers = NULL;
	settings->ServerLicenseProductIssuersCount = 0;
}

BOOL freerdp_server_license_issuers_copy(rdpSettings* settings, char** issuers, UINT32 count)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(issuers || (count == 0));

	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_ServerLicenseProductIssuers, NULL,
	                                      count))
		return FALSE;

	for (UINT32 x = 0; x < count; x++)
	{
		char* issuer = _strdup(issuers[x]);
		if (!issuer)
			return FALSE;
		settings->ServerLicenseProductIssuers[x] = issuer;
	}

	return TRUE;
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
	(void)freerdp_settings_set_uint32(settings, FreeRDP_PerformanceFlags, PerformanceFlags);
}

void freerdp_performance_flags_split(rdpSettings* settings)
{
	(void)freerdp_settings_set_bool(
	    settings, FreeRDP_AllowFontSmoothing,
	    (freerdp_settings_get_uint32(settings, FreeRDP_PerformanceFlags) &
	     PERF_ENABLE_FONT_SMOOTHING)
	        ? TRUE
	        : FALSE);
	(void)freerdp_settings_set_bool(
	    settings, FreeRDP_AllowDesktopComposition,
	    (freerdp_settings_get_uint32(settings, FreeRDP_PerformanceFlags) &
	     PERF_ENABLE_DESKTOP_COMPOSITION)
	        ? TRUE
	        : FALSE);
	(void)freerdp_settings_set_bool(
	    settings, FreeRDP_DisableWallpaper,
	    (freerdp_settings_get_uint32(settings, FreeRDP_PerformanceFlags) & PERF_DISABLE_WALLPAPER)
	        ? TRUE
	        : FALSE);
	(void)freerdp_settings_set_bool(
	    settings, FreeRDP_DisableFullWindowDrag,
	    (freerdp_settings_get_uint32(settings, FreeRDP_PerformanceFlags) &
	     PERF_DISABLE_FULLWINDOWDRAG)
	        ? TRUE
	        : FALSE);
	(void)freerdp_settings_set_bool(
	    settings, FreeRDP_DisableMenuAnims,
	    (freerdp_settings_get_uint32(settings, FreeRDP_PerformanceFlags) &
	     PERF_DISABLE_MENUANIMATIONS)
	        ? TRUE
	        : FALSE);
	(void)freerdp_settings_set_bool(
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

#if defined(WITH_FREERDP_DEPRECATED)
BOOL freerdp_get_param_bool(const rdpSettings* settings, int id)
{
	return freerdp_settings_get_bool(settings, (FreeRDP_Settings_Keys_Bool)id);
}

int freerdp_set_param_bool(rdpSettings* settings, int id, BOOL param)
{
	return freerdp_settings_set_bool(settings, (FreeRDP_Settings_Keys_Bool)id, param) ? 0 : -1;
}

int freerdp_get_param_int(const rdpSettings* settings, int id)
{
	return freerdp_settings_get_int32(settings, (FreeRDP_Settings_Keys_Int32)id);
}

int freerdp_set_param_int(rdpSettings* settings, int id, int param)
{
	return freerdp_settings_set_int32(settings, (FreeRDP_Settings_Keys_Int32)id, param) ? 0 : -1;
}

UINT32 freerdp_get_param_uint32(const rdpSettings* settings, int id)
{
	return freerdp_settings_get_uint32(settings, (FreeRDP_Settings_Keys_UInt32)id);
}

int freerdp_set_param_uint32(rdpSettings* settings, int id, UINT32 param)
{
	return freerdp_settings_set_uint32(settings, (FreeRDP_Settings_Keys_UInt32)id, param) ? 0 : -1;
}

UINT64 freerdp_get_param_uint64(const rdpSettings* settings, int id)
{
	return freerdp_settings_get_uint64(settings, (FreeRDP_Settings_Keys_UInt64)id);
}

int freerdp_set_param_uint64(rdpSettings* settings, int id, UINT64 param)
{
	return freerdp_settings_set_uint64(settings, (FreeRDP_Settings_Keys_UInt64)id, param) ? 0 : -1;
}

char* freerdp_get_param_string(const rdpSettings* settings, int id)
{
	const char* str = freerdp_settings_get_string(settings, (FreeRDP_Settings_Keys_String)id);
	return WINPR_CAST_CONST_PTR_AWAY(str, char*);
}

int freerdp_set_param_string(rdpSettings* settings, int id, const char* param)
{
	return freerdp_settings_set_string(settings, (FreeRDP_Settings_Keys_String)id, param) ? 0 : -1;
}
#endif

static BOOL value_to_uint(const char* value, ULONGLONG* result, ULONGLONG min, ULONGLONG max)
{
	char* endptr = NULL;
	unsigned long long rc = 0;

	if (!value || !result)
		return FALSE;

	errno = 0;
	rc = _strtoui64(value, &endptr, 0);

	if (errno != 0)
		return FALSE;

	if (endptr == value)
		return FALSE;

	if ((rc < min) || (rc > max))
		return FALSE;

	*result = rc;
	return TRUE;
}

static BOOL value_to_int(const char* value, LONGLONG* result, LONGLONG min, LONGLONG max)
{
	char* endptr = NULL;
	long long rc = 0;

	if (!value || !result)
		return FALSE;

	errno = 0;
	rc = _strtoi64(value, &endptr, 0);

	if (errno != 0)
		return FALSE;

	if (endptr == value)
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
	ULONGLONG uval = 0;
	LONGLONG ival = 0;
	SSIZE_T type = 0;

	if (!settings || !name)
		return FALSE;

	const SSIZE_T i = freerdp_settings_get_key_for_name(name);
	if (i < 0)
	{
		WLog_ERR(TAG, "Invalid settings key [%s]", name);
		return FALSE;
	}

	const SSIZE_T index = i;

	type = freerdp_settings_get_type_for_key(index);
	switch (type)
	{

		case RDP_SETTINGS_TYPE_BOOL:
		{
			const BOOL val = (_strnicmp(value, "TRUE", 5) == 0) || (_strnicmp(value, "ON", 5) == 0);
			const BOOL nval =
			    (_strnicmp(value, "FALSE", 6) == 0) || (_strnicmp(value, "OFF", 6) == 0);
			if (!val && !nval)
				return parsing_fail(name, "BOOL", value);

			// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
			return freerdp_settings_set_bool(settings, (FreeRDP_Settings_Keys_Bool)index, val);
		}
		case RDP_SETTINGS_TYPE_UINT16:
			if (!value_to_uint(value, &uval, 0, UINT16_MAX))
				return parsing_fail(name, "UINT16", value);

			// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
			if (!freerdp_settings_set_uint16(settings, (FreeRDP_Settings_Keys_UInt16)index,
			                                 (UINT16)uval))
				return parsing_fail(name, "UINT16", value);
			return TRUE;

		case RDP_SETTINGS_TYPE_INT16:
			if (!value_to_int(value, &ival, INT16_MIN, INT16_MAX))
				return parsing_fail(name, "INT16", value);

			// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
			if (!freerdp_settings_set_int16(settings, (FreeRDP_Settings_Keys_Int16)index,
			                                (INT16)ival))
				return parsing_fail(name, "INT16", value);
			return TRUE;
		case RDP_SETTINGS_TYPE_UINT32:
			if (!value_to_uint(value, &uval, 0, UINT32_MAX))
				return parsing_fail(name, "UINT32", value);

			// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
			if (!freerdp_settings_set_uint32(settings, (FreeRDP_Settings_Keys_UInt32)index,
			                                 (UINT32)uval))
				return parsing_fail(name, "UINT32", value);
			return TRUE;
		case RDP_SETTINGS_TYPE_INT32:
			if (!value_to_int(value, &ival, INT32_MIN, INT32_MAX))
				return parsing_fail(name, "INT32", value);

			// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
			if (!freerdp_settings_set_int32(settings, (FreeRDP_Settings_Keys_Int32)index,
			                                (INT32)ival))
				return parsing_fail(name, "INT32", value);
			return TRUE;
		case RDP_SETTINGS_TYPE_UINT64:
			if (!value_to_uint(value, &uval, 0, UINT64_MAX))
				return parsing_fail(name, "UINT64", value);

			// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
			if (!freerdp_settings_set_uint64(settings, (FreeRDP_Settings_Keys_UInt64)index, uval))
				return parsing_fail(name, "UINT64", value);
			return TRUE;
		case RDP_SETTINGS_TYPE_INT64:
			if (!value_to_int(value, &ival, INT64_MIN, INT64_MAX))
				return parsing_fail(name, "INT64", value);

			// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
			if (!freerdp_settings_set_int64(settings, (FreeRDP_Settings_Keys_Int64)index, ival))
				return parsing_fail(name, "INT64", value);
			return TRUE;

		case RDP_SETTINGS_TYPE_STRING:
			// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
			return freerdp_settings_set_string(settings, (FreeRDP_Settings_Keys_String)index,
			                                   value);
		case RDP_SETTINGS_TYPE_POINTER:
			return parsing_fail(name, "POINTER", value);
		default:
			return FALSE;
	}
	return FALSE;
}

BOOL freerdp_settings_set_pointer_len_(rdpSettings* settings, FreeRDP_Settings_Keys_Pointer id,
                                       FreeRDP_Settings_Keys_UInt32 lenId, const void* data,
                                       size_t len, size_t size)
{
	BOOL rc = FALSE;
	void* copy = NULL;
	void* old = freerdp_settings_get_pointer_writable(settings, id);
	free(old);
	if (!freerdp_settings_set_pointer(settings, id, NULL))
		return FALSE;
	if (lenId != FreeRDP_UINT32_UNUSED)
	{
		if (!freerdp_settings_set_uint32(settings, lenId, 0))
			return FALSE;
	}

	if (len > UINT32_MAX)
		return FALSE;
	if (len == 0)
		return TRUE;
	copy = calloc(len, size);
	if (!copy)
		return FALSE;
	if (data)
		memcpy(copy, data, len * size);
	rc = freerdp_settings_set_pointer(settings, id, copy);
	if (!rc)
	{
		free(copy);
		return FALSE;
	}

	// freerdp_settings_set_pointer takes ownership of copy
	//  NOLINTNEXTLINE(clang-analyzer-unix.Malloc)
	if (lenId == FreeRDP_UINT32_UNUSED)
		return TRUE;
	return freerdp_settings_set_uint32(settings, lenId, (UINT32)len);
}

const void* freerdp_settings_get_pointer(const rdpSettings* settings,
                                         FreeRDP_Settings_Keys_Pointer id)
{
	union
	{
		const rdpSettings* pc;
		rdpSettings* p;
	} cnv;
	cnv.pc = settings;
	return freerdp_settings_get_pointer_writable(cnv.p, id);
}

BOOL freerdp_settings_set_pointer_len(rdpSettings* settings, FreeRDP_Settings_Keys_Pointer id,
                                      const void* data, size_t len)
{
	union
	{
		const void* cv;
		void* v;
	} cnv;

	cnv.cv = data;
	if (!settings)
		return FALSE;

	switch (id)
	{
		case FreeRDP_RdpServerCertificate:
			freerdp_certificate_free(settings->RdpServerCertificate);

			if (len > 1)
			{
				WLog_ERR(TAG, "FreeRDP_RdpServerCertificate::len must be 0 or 1");
				return FALSE;
			}
			settings->RdpServerCertificate = cnv.v;
			if (!settings->RdpServerCertificate && (len > 0))
			{
				settings->RdpServerCertificate = freerdp_certificate_new();
				if (!settings->RdpServerCertificate)
					return FALSE;
			}
			return TRUE;
		case FreeRDP_RdpServerRsaKey:
			freerdp_key_free(settings->RdpServerRsaKey);
			if (len > 1)
			{
				WLog_ERR(TAG, "FreeRDP_RdpServerRsaKey::len must be 0 or 1");
				return FALSE;
			}
			settings->RdpServerRsaKey = (rdpPrivateKey*)cnv.v;
			if (!settings->RdpServerRsaKey && (len > 0))
			{
				settings->RdpServerRsaKey = freerdp_key_new();
				if (!settings->RdpServerRsaKey)
					return FALSE;
			}
			return TRUE;
		case FreeRDP_RedirectionPassword:
			return freerdp_settings_set_pointer_len_(
			    settings, id, FreeRDP_RedirectionPasswordLength, data, len, sizeof(char));
		case FreeRDP_RedirectionTsvUrl:
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_RedirectionTsvUrlLength,
			                                         data, len, sizeof(char));
		case FreeRDP_RedirectionTargetCertificate:
			freerdp_certificate_free(settings->RedirectionTargetCertificate);

			if (len > 1)
			{
				WLog_ERR(TAG, "FreeRDP_RedirectionTargetCertificate::len must be 0 or 1");
				return FALSE;
			}
			settings->RedirectionTargetCertificate = cnv.v;
			if (!settings->RedirectionTargetCertificate && (len > 0))
			{
				settings->RedirectionTargetCertificate = freerdp_certificate_new();
				if (!settings->RedirectionTargetCertificate)
					return FALSE;
			}
			return TRUE;
		case FreeRDP_RedirectionGuid:
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_RedirectionGuidLength,
			                                         data, len, sizeof(BYTE));
		case FreeRDP_LoadBalanceInfo:
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_LoadBalanceInfoLength,
			                                         data, len, sizeof(char));
		case FreeRDP_ServerRandom:
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_ServerRandomLength, data,
			                                         len, sizeof(char));
		case FreeRDP_ClientRandom:
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_ClientRandomLength, data,
			                                         len, sizeof(char));
		case FreeRDP_ServerCertificate:
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_ServerCertificateLength,
			                                         data, len, sizeof(char));
		case FreeRDP_TargetNetAddresses:
			if (!freerdp_target_net_addresses_resize(settings, len))
				return FALSE;
			if (data == NULL)
				target_net_addresses_free(settings, 0);
			return TRUE;
		case FreeRDP_ServerLicenseProductIssuers:
			if (data == NULL)
				freerdp_server_license_issuers_free(settings);
			return freerdp_settings_set_pointer_len_(
			    settings, id, FreeRDP_ServerLicenseProductIssuersCount, data, len, sizeof(char*));
		case FreeRDP_TargetNetPorts:
			if (!freerdp_target_net_addresses_resize(settings, len))
				return FALSE;
			if (data == NULL)
			{
				for (size_t x = 0; x < len; x++)
					settings->TargetNetPorts[x] = 0;
			}
			return TRUE;
		case FreeRDP_DeviceArray:
			if (data == NULL)
				freerdp_device_collection_free(settings);
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_DeviceArraySize, data,
			                                         len, sizeof(RDPDR_DEVICE*));
		case FreeRDP_ChannelDefArray:
			if ((len > 0) && (len < CHANNEL_MAX_COUNT))
				WLog_WARN(TAG,
				          "FreeRDP_ChannelDefArray::len expected to be >= %d, but have %" PRIuz,
				          CHANNEL_MAX_COUNT, len);
			return freerdp_settings_set_pointer_len_(settings, FreeRDP_ChannelDefArray,
			                                         FreeRDP_ChannelDefArraySize, data, len,
			                                         sizeof(CHANNEL_DEF));
		case FreeRDP_MonitorDefArray:
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_MonitorDefArraySize,
			                                         data, len, sizeof(rdpMonitor));
		case FreeRDP_ClientAutoReconnectCookie:
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_UINT32_UNUSED, data, len,
			                                         sizeof(ARC_CS_PRIVATE_PACKET));
		case FreeRDP_ServerAutoReconnectCookie:
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_UINT32_UNUSED, data, len,
			                                         sizeof(ARC_SC_PRIVATE_PACKET));
		case FreeRDP_ClientTimeZone:
			if (len > 1)
			{
				WLog_ERR(TAG, "FreeRDP_ClientTimeZone::len must be 0 or 1");
				return FALSE;
			}
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_UINT32_UNUSED, data, len,
			                                         sizeof(TIME_ZONE_INFORMATION));
		case FreeRDP_BitmapCacheV2CellInfo:
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_BitmapCacheV2NumCells,
			                                         data, len, sizeof(BITMAP_CACHE_V2_CELL_INFO));
		case FreeRDP_GlyphCache:
			if ((len != 0) && (len != 10))
			{
				WLog_ERR(TAG, "FreeRDP_GlyphCache::len must be 0 or 10");
				return FALSE;
			}
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_UINT32_UNUSED, data, len,
			                                         sizeof(GLYPH_CACHE_DEFINITION));
		case FreeRDP_FragCache:
			if (len > 1)
			{
				WLog_ERR(TAG, "FreeRDP_FragCache::len must be 0 or 1");
				return FALSE;
			}
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_UINT32_UNUSED, data, len,
			                                         sizeof(GLYPH_CACHE_DEFINITION));
		case FreeRDP_StaticChannelArray:
			if (data == NULL)
				freerdp_static_channel_collection_free(settings);
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_StaticChannelArraySize,
			                                         data, len, sizeof(ADDIN_ARGV*));
		case FreeRDP_DynamicChannelArray:
			if (data == NULL)
				freerdp_dynamic_channel_collection_free(settings);
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_DynamicChannelArraySize,
			                                         data, len, sizeof(ADDIN_ARGV*));
		case FreeRDP_ReceivedCapabilityData:
			if (!freerdp_capability_buffer_resize(settings, len, FALSE))
				return FALSE;
			if (data == NULL)
			{
				freerdp_capability_data_free(settings, 0, FALSE);
			}
			return TRUE;
		case FreeRDP_ReceivedCapabilities:
			if (!freerdp_capability_buffer_resize(settings, len, FALSE))
				return FALSE;
			if (data == NULL)
			{
				for (size_t x = 0; x < settings->ReceivedCapabilitiesSize; x++)
				{
					settings->ReceivedCapabilities[x] = 0;
				}
			}
			return TRUE;
		case FreeRDP_OrderSupport:
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_UINT32_UNUSED, data, len,
			                                         sizeof(char));

		case FreeRDP_MonitorIds:
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_NumMonitorIds, data, len,
			                                         sizeof(UINT32));

		case FreeRDP_ReceivedCapabilityDataSizes:
			if (!freerdp_capability_buffer_resize(settings, len, FALSE))
				return FALSE;
			if (data == NULL)
			{
				for (size_t x = 0; x < settings->ReceivedCapabilitiesSize; x++)
					settings->ReceivedCapabilityDataSizes[x] = 0;
			}
			return TRUE;

		case FreeRDP_Password51:
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_Password51Length, data,
			                                         len, sizeof(char));
		default:
			if ((data == NULL) && (len == 0))
			{
				freerdp_settings_set_pointer(settings, id, NULL);
			}
			else
				WLog_WARN(TAG, "Invalid id %d", id);
			return FALSE;
	}
}

void* freerdp_settings_get_pointer_array_writable(const rdpSettings* settings,
                                                  FreeRDP_Settings_Keys_Pointer id, size_t offset)
{
	size_t max = 0;
	if (!settings)
		return NULL;
	switch (id)
	{
		case FreeRDP_ClientAutoReconnectCookie:
			max = 1;
			if ((offset >= max) || !settings->ClientAutoReconnectCookie)
				goto fail;
			return &settings->ClientAutoReconnectCookie[offset];
		case FreeRDP_ServerAutoReconnectCookie:
			max = 1;
			if ((offset >= max) || !settings->ServerAutoReconnectCookie)
				goto fail;
			return &settings->ServerAutoReconnectCookie[offset];
		case FreeRDP_ServerCertificate:
			max = freerdp_settings_get_uint32(settings, FreeRDP_ServerCertificateLength);
			if (offset >= max)
				goto fail;
			return &settings->ServerCertificate[offset];
		case FreeRDP_ServerRandom:
			max = freerdp_settings_get_uint32(settings, FreeRDP_ServerRandomLength);
			if (offset >= max)
				goto fail;
			return &settings->ServerRandom[offset];
		case FreeRDP_ClientRandom:
			max = freerdp_settings_get_uint32(settings, FreeRDP_ClientRandomLength);
			if (offset >= max)
				goto fail;
			return &settings->ClientRandom[offset];
		case FreeRDP_LoadBalanceInfo:
			max = freerdp_settings_get_uint32(settings, FreeRDP_LoadBalanceInfoLength);
			if (offset >= max)
				goto fail;
			return &settings->LoadBalanceInfo[offset];

		case FreeRDP_RedirectionTsvUrl:
			max = freerdp_settings_get_uint32(settings, FreeRDP_RedirectionTsvUrlLength);
			if (offset >= max)
				goto fail;
			return &settings->RedirectionTsvUrl[offset];

		case FreeRDP_RedirectionPassword:
			max = freerdp_settings_get_uint32(settings, FreeRDP_RedirectionPasswordLength);
			if (offset >= max)
				goto fail;
			return &settings->RedirectionPassword[offset];

		case FreeRDP_OrderSupport:
			max = 32;
			if (offset >= max)
				goto fail;
			return &settings->OrderSupport[offset];
		case FreeRDP_MonitorIds:
			max = freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorIds);
			if (offset >= max)
				goto fail;
			return &settings->MonitorIds[offset];
		case FreeRDP_MonitorDefArray:
			max = freerdp_settings_get_uint32(settings, FreeRDP_MonitorDefArraySize);
			if (offset >= max)
				goto fail;
			return &settings->MonitorDefArray[offset];
		case FreeRDP_ChannelDefArray:
			max = freerdp_settings_get_uint32(settings, FreeRDP_ChannelDefArraySize);
			if (offset >= max)
				goto fail;
			return &settings->ChannelDefArray[offset];
		case FreeRDP_DeviceArray:
			max = freerdp_settings_get_uint32(settings, FreeRDP_DeviceArraySize);
			if (offset >= max)
				goto fail;
			return settings->DeviceArray[offset];
		case FreeRDP_StaticChannelArray:
			max = freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelArraySize);
			if (offset >= max)
				goto fail;
			return settings->StaticChannelArray[offset];
		case FreeRDP_DynamicChannelArray:
			max = freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelArraySize);
			if (offset >= max)
				goto fail;
			return settings->DynamicChannelArray[offset];
		case FreeRDP_FragCache:
			max = 1;
			if (offset >= max)
				goto fail;
			return &settings->FragCache[offset];
		case FreeRDP_GlyphCache:
			max = 10;
			if (offset >= max)
				goto fail;
			return &settings->GlyphCache[offset];
		case FreeRDP_BitmapCacheV2CellInfo:
			max = freerdp_settings_get_uint32(settings, FreeRDP_BitmapCacheV2NumCells);
			if (offset >= max)
				goto fail;
			return &settings->BitmapCacheV2CellInfo[offset];
		case FreeRDP_ReceivedCapabilities:
			max = freerdp_settings_get_uint32(settings, FreeRDP_ReceivedCapabilitiesSize);
			if (offset >= max)
				goto fail;
			return &settings->ReceivedCapabilities[offset];
		case FreeRDP_TargetNetAddresses:
			max = freerdp_settings_get_uint32(settings, FreeRDP_TargetNetAddressCount);
			if (offset >= max)
				goto fail;
			WINPR_ASSERT(settings->TargetNetAddresses);
			return settings->TargetNetAddresses[offset];
		case FreeRDP_TargetNetPorts:
			max = freerdp_settings_get_uint32(settings, FreeRDP_TargetNetAddressCount);
			if (offset >= max)
				goto fail;
			WINPR_ASSERT(settings->TargetNetPorts);
			return &settings->TargetNetPorts[offset];
		case FreeRDP_ClientTimeZone:
			max = 1;
			if (offset >= max)
				goto fail;
			return settings->ClientTimeZone;
		case FreeRDP_RdpServerCertificate:
			max = 1;
			if (offset >= max)
				goto fail;
			return settings->RdpServerCertificate;
		case FreeRDP_RdpServerRsaKey:
			max = 1;
			if (offset >= max)
				goto fail;
			return settings->RdpServerRsaKey;
		case FreeRDP_ServerLicenseProductIssuers:
			max = freerdp_settings_get_uint32(settings, FreeRDP_ServerLicenseProductIssuersCount);
			if (offset >= max)
				goto fail;
			return settings->ServerLicenseProductIssuers[offset];
		case FreeRDP_ReceivedCapabilityData:
			max = freerdp_settings_get_uint32(settings, FreeRDP_ReceivedCapabilitiesSize);
			if (offset >= max)
				goto fail;
			WINPR_ASSERT(settings->ReceivedCapabilityData);
			return settings->ReceivedCapabilityData[offset];

		case FreeRDP_ReceivedCapabilityDataSizes:
			max = freerdp_settings_get_uint32(settings, FreeRDP_ReceivedCapabilitiesSize);
			if (offset >= max)
				goto fail;
			WINPR_ASSERT(settings->ReceivedCapabilityDataSizes);
			return &settings->ReceivedCapabilityDataSizes[offset];
		default:
			WLog_WARN(TAG, "Invalid id %s [%d]", freerdp_settings_get_name_for_key(id), id);
			return NULL;
	}

fail:
	WLog_WARN(TAG, "Invalid offset for %s [%d]: size=%" PRIuz ", offset=%" PRIuz,
	          freerdp_settings_get_name_for_key(id), id, max, offset);
	return NULL;
}

BOOL freerdp_settings_set_pointer_array(rdpSettings* settings, FreeRDP_Settings_Keys_Pointer id,
                                        size_t offset, const void* data)
{
	size_t maxOffset = 0;
	if (!settings)
		return FALSE;
	switch (id)
	{
		case FreeRDP_ClientAutoReconnectCookie:
			maxOffset = 1;
			if ((offset >= maxOffset) || !data || !settings->ClientAutoReconnectCookie)
				goto fail;
			settings->ClientAutoReconnectCookie[offset] = *(const ARC_CS_PRIVATE_PACKET*)data;
			return TRUE;
		case FreeRDP_ServerAutoReconnectCookie:
			maxOffset = 1;
			if ((offset >= maxOffset) || !data || !settings->ServerAutoReconnectCookie)
				goto fail;
			settings->ServerAutoReconnectCookie[offset] = *(const ARC_SC_PRIVATE_PACKET*)data;
			return TRUE;
		case FreeRDP_ServerCertificate:
			maxOffset = freerdp_settings_get_uint32(settings, FreeRDP_ServerCertificateLength);
			if ((offset >= maxOffset) || !data)
				goto fail;
			settings->ServerCertificate[offset] = *(const BYTE*)data;
			return TRUE;
		case FreeRDP_DeviceArray:
			maxOffset = freerdp_settings_get_uint32(settings, FreeRDP_DeviceArraySize);
			if (offset >= maxOffset)
				goto fail;
			freerdp_device_free(settings->DeviceArray[offset]);
			settings->DeviceArray[offset] = freerdp_device_clone(data);
			return TRUE;
		case FreeRDP_TargetNetAddresses:
			maxOffset = freerdp_settings_get_uint32(settings, FreeRDP_TargetNetAddressCount);
			if ((offset >= maxOffset) || !data)
				goto fail;
			WINPR_ASSERT(settings->TargetNetAddresses);
			free(settings->TargetNetAddresses[offset]);
			settings->TargetNetAddresses[offset] = _strdup((const char*)data);
			return settings->TargetNetAddresses[offset] != NULL;
		case FreeRDP_TargetNetPorts:
			maxOffset = freerdp_settings_get_uint32(settings, FreeRDP_TargetNetAddressCount);
			if ((offset >= maxOffset) || !data)
				goto fail;
			WINPR_ASSERT(settings->TargetNetPorts);
			settings->TargetNetPorts[offset] = *((const UINT32*)data);
			return TRUE;
		case FreeRDP_StaticChannelArray:
			maxOffset = freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelArraySize);
			if ((offset >= maxOffset) || !data)
				goto fail;
			freerdp_addin_argv_free(settings->StaticChannelArray[offset]);
			settings->StaticChannelArray[offset] = freerdp_addin_argv_clone(data);
			return TRUE;
		case FreeRDP_DynamicChannelArray:
			maxOffset = freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelArraySize);
			if ((offset >= maxOffset) || !data)
				goto fail;
			freerdp_addin_argv_free(settings->DynamicChannelArray[offset]);
			settings->DynamicChannelArray[offset] = freerdp_addin_argv_clone(data);
			return TRUE;
		case FreeRDP_BitmapCacheV2CellInfo:
			maxOffset = freerdp_settings_get_uint32(settings, FreeRDP_BitmapCacheV2NumCells);
			if ((offset >= maxOffset) || !data)
				goto fail;
			{
				const BITMAP_CACHE_V2_CELL_INFO* cdata = (const BITMAP_CACHE_V2_CELL_INFO*)data;
				settings->BitmapCacheV2CellInfo[offset] = *cdata;
			}
			return TRUE;
		case FreeRDP_ServerRandom:
			maxOffset = freerdp_settings_get_uint32(settings, FreeRDP_ServerRandomLength);
			if ((offset >= maxOffset) || !data)
				goto fail;
			settings->ServerRandom[offset] = *(const BYTE*)data;
			return TRUE;
		case FreeRDP_ClientRandom:
			maxOffset = freerdp_settings_get_uint32(settings, FreeRDP_ClientRandomLength);
			if ((offset >= maxOffset) || !data)
				goto fail;
			settings->ClientRandom[offset] = *(const BYTE*)data;
			return TRUE;
		case FreeRDP_LoadBalanceInfo:
			maxOffset = freerdp_settings_get_uint32(settings, FreeRDP_LoadBalanceInfoLength);
			if ((offset >= maxOffset) || !data)
				goto fail;
			settings->LoadBalanceInfo[offset] = *(const BYTE*)data;
			return TRUE;
		case FreeRDP_RedirectionTsvUrl:
			maxOffset = freerdp_settings_get_uint32(settings, FreeRDP_RedirectionTsvUrlLength);
			if ((offset >= maxOffset) || !data)
				goto fail;
			settings->RedirectionTsvUrl[offset] = *(const BYTE*)data;
			return TRUE;
		case FreeRDP_RedirectionPassword:
			maxOffset = freerdp_settings_get_uint32(settings, FreeRDP_RedirectionPasswordLength);
			if ((offset >= maxOffset) || !data)
				goto fail;
			settings->RedirectionPassword[offset] = *(const BYTE*)data;
			return TRUE;
		case FreeRDP_OrderSupport:
			maxOffset = 32;
			if (!settings->OrderSupport)
				goto fail;
			if ((offset >= maxOffset) || !data)
				goto fail;
			settings->OrderSupport[offset] = *(const BOOL*)data ? 1 : 0;
			return TRUE;
		case FreeRDP_GlyphCache:
			maxOffset = 10;
			if (!settings->GlyphCache)
				goto fail;
			if ((offset >= maxOffset) || !data)
				goto fail;
			settings->GlyphCache[offset] = *(const GLYPH_CACHE_DEFINITION*)data;
			return TRUE;
		case FreeRDP_FragCache:
			maxOffset = 1;
			if (!settings->FragCache)
				goto fail;
			if ((offset >= maxOffset) || !data)
				goto fail;
			settings->FragCache[offset] = *(const GLYPH_CACHE_DEFINITION*)data;
			return TRUE;
		case FreeRDP_MonitorIds:
			maxOffset = freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorIds);
			if ((offset >= maxOffset) || !data)
				goto fail;
			settings->MonitorIds[offset] = *(const UINT32*)data;
			return TRUE;
		case FreeRDP_ChannelDefArray:
			maxOffset = freerdp_settings_get_uint32(settings, FreeRDP_ChannelDefArraySize);
			if ((offset >= maxOffset) || !data)
				goto fail;
			settings->ChannelDefArray[offset] = *(const CHANNEL_DEF*)data;
			return TRUE;
		case FreeRDP_MonitorDefArray:
			maxOffset = freerdp_settings_get_uint32(settings, FreeRDP_MonitorDefArraySize);
			if ((offset >= maxOffset) || !data)
				goto fail;
			settings->MonitorDefArray[offset] = *(const rdpMonitor*)data;
			return TRUE;

		case FreeRDP_ClientTimeZone:
			maxOffset = 1;
			if ((offset >= maxOffset) || !data || !settings->ClientTimeZone)
				goto fail;
			settings->ClientTimeZone[offset] = *(const TIME_ZONE_INFORMATION*)data;
			return TRUE;

		case FreeRDP_ServerLicenseProductIssuers:
			maxOffset =
			    freerdp_settings_get_uint32(settings, FreeRDP_ServerLicenseProductIssuersCount);
			if ((offset >= maxOffset) || !settings->ServerLicenseProductIssuers)
				goto fail;
			free(settings->ServerLicenseProductIssuers[offset]);
			settings->ServerLicenseProductIssuers[offset] = NULL;
			if (data)
				settings->ServerLicenseProductIssuers[offset] = _strdup((const char*)data);
			return TRUE;

		case FreeRDP_ReceivedCapabilityData:
			maxOffset = freerdp_settings_get_uint32(settings, FreeRDP_ReceivedCapabilitiesSize);
			if (offset >= maxOffset)
				goto fail;
			WINPR_ASSERT(settings->ReceivedCapabilityData);
			settings->ReceivedCapabilityData[offset] = WINPR_CAST_CONST_PTR_AWAY(data, BYTE*);
			return TRUE;
		case FreeRDP_ReceivedCapabilityDataSizes:
			maxOffset = freerdp_settings_get_uint32(settings, FreeRDP_ReceivedCapabilitiesSize);
			if (offset >= maxOffset)
				goto fail;
			WINPR_ASSERT(settings->ReceivedCapabilityDataSizes);
			settings->ReceivedCapabilityDataSizes[offset] = *(const uint32_t*)data;
			return TRUE;
		default:
			WLog_WARN(TAG, "Invalid id %s [%d]", freerdp_settings_get_name_for_key(id), id);
			return FALSE;
	}

fail:
	WLog_WARN(TAG, "[%s] Invalid offset=%" PRIuz " [%" PRIuz "] or NULL data=%p",
	          freerdp_settings_get_name_for_key(id), offset, maxOffset, data);
	return FALSE;
}

const void* freerdp_settings_get_pointer_array(const rdpSettings* settings,
                                               FreeRDP_Settings_Keys_Pointer id, size_t offset)
{
	return freerdp_settings_get_pointer_array_writable(settings, id, offset);
}

UINT32 freerdp_settings_get_codecs_flags(const rdpSettings* settings)
{
	UINT32 flags = FREERDP_CODEC_ALL;
	if (settings->RemoteFxCodec == FALSE)
	{
		flags &= (uint32_t)~FREERDP_CODEC_REMOTEFX;
	}
	if (settings->NSCodec == FALSE)
	{
		flags &= (uint32_t)~FREERDP_CODEC_NSCODEC;
	}
	/*TODO: check other codecs flags */
	return flags;
}

const char* freerdp_settings_get_server_name(const rdpSettings* settings)
{
	WINPR_ASSERT(settings);
	const char* hostname = settings->ServerHostname;

	if (settings->UserSpecifiedServerName)
		hostname = settings->UserSpecifiedServerName;

	return hostname;
}

#if defined(WITH_FREERDP_DEPRECATED)
ADDIN_ARGV* freerdp_static_channel_clone(ADDIN_ARGV* channel)
{
	return freerdp_addin_argv_clone(channel);
}

ADDIN_ARGV* freerdp_dynamic_channel_clone(ADDIN_ARGV* channel)
{
	return freerdp_addin_argv_clone(channel);
}
#endif

BOOL freerdp_target_net_addresses_copy(rdpSettings* settings, char** addresses, UINT32 count)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(addresses);

	if (!freerdp_target_net_adresses_reset(settings, count))
		return FALSE;

	for (UINT32 i = 0; i < settings->TargetNetAddressCount; i++)
	{
		const char* address = addresses[i];
		if (!freerdp_settings_set_pointer_array(settings, FreeRDP_TargetNetAddresses, i, address))
		{
			freerdp_target_net_addresses_free(settings);
			return FALSE;
		}
	}

	return TRUE;
}

BOOL freerdp_device_equal(const RDPDR_DEVICE* what, const RDPDR_DEVICE* expect)
{
	if (!what && !expect)
		return TRUE;
	if (!what || !expect)
		return FALSE;

	if (what->Id != expect->Id)
		return FALSE;
	if (what->Type != expect->Type)
		return FALSE;
	if (what->Name && expect->Name)
	{
		if (strcmp(what->Name, expect->Name) != 0)
			return FALSE;
	}
	else
	{
		if (what->Name != expect->Name)
			return FALSE;
	}

	switch (what->Type)
	{
		case RDPDR_DTYP_PRINT:
		{
			const RDPDR_PRINTER* a = (const RDPDR_PRINTER*)what;
			const RDPDR_PRINTER* b = (const RDPDR_PRINTER*)expect;
			if (a->DriverName && b->DriverName)
				return strcmp(a->DriverName, b->DriverName) == 0;
			return a->DriverName == b->DriverName;
		}

		case RDPDR_DTYP_SERIAL:
		{
			const RDPDR_SERIAL* a = (const RDPDR_SERIAL*)what;
			const RDPDR_SERIAL* b = (const RDPDR_SERIAL*)expect;

			if (a->Path && b->Path)
			{
				if (strcmp(a->Path, b->Path) != 0)
					return FALSE;
			}
			else if (a->Path != b->Path)
				return FALSE;

			if (a->Driver && b->Driver)
			{
				if (strcmp(a->Driver, b->Driver) != 0)
					return FALSE;
			}
			else if (a->Driver != b->Driver)
				return FALSE;
			if (a->Permissive && b->Permissive)
				return strcmp(a->Permissive, b->Permissive) == 0;
			return a->Permissive == b->Permissive;
		}

		case RDPDR_DTYP_PARALLEL:
		{
			const RDPDR_PARALLEL* a = (const RDPDR_PARALLEL*)what;
			const RDPDR_PARALLEL* b = (const RDPDR_PARALLEL*)expect;
			if (a->Path && b->Path)
				return strcmp(a->Path, b->Path) == 0;
			return a->Path == b->Path;
		}

		case RDPDR_DTYP_SMARTCARD:
			break;
		case RDPDR_DTYP_FILESYSTEM:
		{
			const RDPDR_DRIVE* a = (const RDPDR_DRIVE*)what;
			const RDPDR_DRIVE* b = (const RDPDR_DRIVE*)expect;
			if (a->automount != b->automount)
				return FALSE;
			if (a->Path && b->Path)
				return strcmp(a->Path, b->Path) == 0;
			return a->Path == b->Path;
		}

		default:
			return FALSE;
	}

	return TRUE;
}

const char* freerdp_rail_support_flags_to_string(UINT32 flags, char* buffer, size_t length)
{
	const UINT32 mask =
	    RAIL_LEVEL_SUPPORTED | RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED |
	    RAIL_LEVEL_SHELL_INTEGRATION_SUPPORTED | RAIL_LEVEL_LANGUAGE_IME_SYNC_SUPPORTED |
	    RAIL_LEVEL_SERVER_TO_CLIENT_IME_SYNC_SUPPORTED | RAIL_LEVEL_HIDE_MINIMIZED_APPS_SUPPORTED |
	    RAIL_LEVEL_WINDOW_CLOAKING_SUPPORTED | RAIL_LEVEL_HANDSHAKE_EX_SUPPORTED;

	if (flags & RAIL_LEVEL_SUPPORTED)
		winpr_str_append("RAIL_LEVEL_SUPPORTED", buffer, length, "|");
	if (flags & RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED)
		winpr_str_append("RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED", buffer, length, "|");
	if (flags & RAIL_LEVEL_SHELL_INTEGRATION_SUPPORTED)
		winpr_str_append("RAIL_LEVEL_SHELL_INTEGRATION_SUPPORTED", buffer, length, "|");
	if (flags & RAIL_LEVEL_LANGUAGE_IME_SYNC_SUPPORTED)
		winpr_str_append("RAIL_LEVEL_LANGUAGE_IME_SYNC_SUPPORTED", buffer, length, "|");
	if (flags & RAIL_LEVEL_SERVER_TO_CLIENT_IME_SYNC_SUPPORTED)
		winpr_str_append("RAIL_LEVEL_SERVER_TO_CLIENT_IME_SYNC_SUPPORTED", buffer, length, "|");
	if (flags & RAIL_LEVEL_HIDE_MINIMIZED_APPS_SUPPORTED)
		winpr_str_append("RAIL_LEVEL_HIDE_MINIMIZED_APPS_SUPPORTED", buffer, length, "|");
	if (flags & RAIL_LEVEL_WINDOW_CLOAKING_SUPPORTED)
		winpr_str_append("RAIL_LEVEL_WINDOW_CLOAKING_SUPPORTED", buffer, length, "|");
	if (flags & RAIL_LEVEL_HANDSHAKE_EX_SUPPORTED)
		winpr_str_append("RAIL_LEVEL_HANDSHAKE_EX_SUPPORTED", buffer, length, "|");
	if (flags & RAIL_LEVEL_LANGUAGE_IME_SYNC_SUPPORTED)
		winpr_str_append("RAIL_LEVEL_LANGUAGE_IME_SYNC_SUPPORTED", buffer, length, "|");
	if ((flags & ~mask) != 0)
	{
		char tbuffer[64] = { 0 };
		(void)_snprintf(tbuffer, sizeof(tbuffer), "RAIL_FLAG_UNKNOWN 0x%08" PRIx32, flags & mask);
		winpr_str_append(tbuffer, buffer, length, "|");
	}
	return buffer;
}

BOOL freerdp_settings_update_from_caps(rdpSettings* settings, const BYTE* capsFlags,
                                       const BYTE** capsData, const UINT32* capsSizes,
                                       UINT32 capsCount, BOOL serverReceivedCaps)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(capsFlags || (capsCount == 0));
	WINPR_ASSERT(capsData || (capsCount == 0));
	WINPR_ASSERT(capsSizes || (capsCount == 0));
	WINPR_ASSERT(capsCount <= UINT16_MAX);

	wLog* log = WLog_Get(TAG);

	for (UINT32 x = 0; x < capsCount; x++)
	{
		if (capsFlags[x])
		{
			wStream buffer = { 0 };
			wStream* sub = Stream_StaticConstInit(&buffer, capsData[x], capsSizes[x]);

			if (!rdp_read_capability_set(log, sub, (UINT16)x, settings, serverReceivedCaps))
				return FALSE;
		}
	}

	return TRUE;
}

const char* freerdp_rdp_version_string(UINT32 version)
{
	switch (version)
	{
		case RDP_VERSION_4:
			return "RDP_VERSION_4";
		case RDP_VERSION_5_PLUS:
			return "RDP_VERSION_5_PLUS";
		case RDP_VERSION_10_0:
			return "RDP_VERSION_10_0";
		case RDP_VERSION_10_1:
			return "RDP_VERSION_10_1";
		case RDP_VERSION_10_2:
			return "RDP_VERSION_10_2";
		case RDP_VERSION_10_3:
			return "RDP_VERSION_10_3";
		case RDP_VERSION_10_4:
			return "RDP_VERSION_10_4";
		case RDP_VERSION_10_5:
			return "RDP_VERSION_10_5";
		case RDP_VERSION_10_6:
			return "RDP_VERSION_10_6";
		case RDP_VERSION_10_7:
			return "RDP_VERSION_10_7";
		case RDP_VERSION_10_8:
			return "RDP_VERSION_10_8";
		case RDP_VERSION_10_9:
			return "RDP_VERSION_10_9";
		case RDP_VERSION_10_10:
			return "RDP_VERSION_10_10";
		case RDP_VERSION_10_11:
			return "RDP_VERSION_10_11";
		case RDP_VERSION_10_12:
			return "RDP_VERSION_10_12";
		default:
			return "RDP_VERSION_UNKNOWN";
	}
}

BOOL freerdp_settings_set_string_from_utf16(rdpSettings* settings, FreeRDP_Settings_Keys_String id,
                                            const WCHAR* param)
{
	WINPR_ASSERT(settings);

	if (!param)
		return freerdp_settings_set_string_copy_(settings, id, NULL, 0, TRUE);

	size_t len = 0;

	char* str = ConvertWCharToUtf8Alloc(param, &len);
	if (!str && (len != 0))
		return FALSE;

	return freerdp_settings_set_string_(settings, id, str, len);
}

BOOL freerdp_settings_set_string_from_utf16N(rdpSettings* settings, FreeRDP_Settings_Keys_String id,
                                             const WCHAR* param, size_t length)
{
	size_t len = 0;

	WINPR_ASSERT(settings);

	if (!param)
		return freerdp_settings_set_string_copy_(settings, id, NULL, length, TRUE);

	char* str = ConvertWCharNToUtf8Alloc(param, length, &len);
	if (!str && (length != 0))
	{
		/* If the input string is an empty string, but length > 0
		 * consider the conversion a success */
		const size_t wlen = _wcsnlen(param, length);
		if (wlen != 0)
			return FALSE;
	}

	return freerdp_settings_set_string_(settings, id, str, len);
}

WCHAR* freerdp_settings_get_string_as_utf16(const rdpSettings* settings,
                                            FreeRDP_Settings_Keys_String id, size_t* pCharLen)
{
	const char* str = freerdp_settings_get_string(settings, id);
	if (pCharLen)
		*pCharLen = 0;
	if (!str)
		return NULL;
	return ConvertUtf8ToWCharAlloc(str, pCharLen);
}

const char* freerdp_rdpdr_dtyp_string(UINT32 type)
{
	switch (type)
	{
		case RDPDR_DTYP_FILESYSTEM:
			return "RDPDR_DTYP_FILESYSTEM";
		case RDPDR_DTYP_PARALLEL:
			return "RDPDR_DTYP_PARALLEL";
		case RDPDR_DTYP_PRINT:
			return "RDPDR_DTYP_PRINT";
		case RDPDR_DTYP_SERIAL:
			return "RDPDR_DTYP_SERIAL";
		case RDPDR_DTYP_SMARTCARD:
			return "RDPDR_DTYP_SMARTCARD";
		default:
			return "RDPDR_DTYP_UNKNOWN";
	}
}

const char* freerdp_encryption_level_string(UINT32 EncryptionLevel)
{
	switch (EncryptionLevel)
	{
		case ENCRYPTION_LEVEL_NONE:
			return "ENCRYPTION_LEVEL_NONE";
		case ENCRYPTION_LEVEL_LOW:
			return "ENCRYPTION_LEVEL_LOW";
		case ENCRYPTION_LEVEL_CLIENT_COMPATIBLE:
			return "ENCRYPTION_LEVEL_CLIENT_COMPATIBLE";
		case ENCRYPTION_LEVEL_HIGH:
			return "ENCRYPTION_LEVEL_HIGH";
		case ENCRYPTION_LEVEL_FIPS:
			return "ENCRYPTION_LEVEL_FIPS";
		default:
			return "ENCRYPTION_LEVEL_UNKNOWN";
	}
}

const char* freerdp_encryption_methods_string(UINT32 EncryptionMethods, char* buffer, size_t size)
{
	if (EncryptionMethods == ENCRYPTION_METHOD_NONE)
	{
		winpr_str_append("ENCRYPTION_METHOD_NONE", buffer, size, "|");
		return buffer;
	}

	if (EncryptionMethods & ENCRYPTION_METHOD_40BIT)
	{
		winpr_str_append("ENCRYPTION_METHOD_40BIT", buffer, size, "|");
	}
	if (EncryptionMethods & ENCRYPTION_METHOD_128BIT)
	{
		winpr_str_append("ENCRYPTION_METHOD_128BIT", buffer, size, "|");
	}
	if (EncryptionMethods & ENCRYPTION_METHOD_56BIT)
	{
		winpr_str_append("ENCRYPTION_METHOD_56BIT", buffer, size, "|");
	}
	if (EncryptionMethods & ENCRYPTION_METHOD_FIPS)
	{
		winpr_str_append("ENCRYPTION_METHOD_FIPS", buffer, size, "|");
	}

	return buffer;
}

const char* freerdp_supported_color_depths_string(UINT16 mask, char* buffer, size_t size)
{
	const UINT32 invalid = mask & ~(RNS_UD_32BPP_SUPPORT | RNS_UD_24BPP_SUPPORT |
	                                RNS_UD_16BPP_SUPPORT | RNS_UD_15BPP_SUPPORT);

	if (mask & RNS_UD_32BPP_SUPPORT)
		winpr_str_append("RNS_UD_32BPP_SUPPORT", buffer, size, "|");
	if (mask & RNS_UD_24BPP_SUPPORT)
		winpr_str_append("RNS_UD_24BPP_SUPPORT", buffer, size, "|");
	if (mask & RNS_UD_16BPP_SUPPORT)
		winpr_str_append("RNS_UD_16BPP_SUPPORT", buffer, size, "|");
	if (mask & RNS_UD_15BPP_SUPPORT)
		winpr_str_append("RNS_UD_15BPP_SUPPORT", buffer, size, "|");

	if (invalid != 0)
	{
		char str[32] = { 0 };
		(void)_snprintf(str, sizeof(str), "RNS_UD_INVALID[0x%04" PRIx32 "]", invalid);
		winpr_str_append(str, buffer, size, "|");
	}
	char hex[32] = { 0 };
	(void)_snprintf(hex, sizeof(hex), "[0x%04" PRIx16 "]", mask);
	return buffer;
}

BOOL freerdp_settings_append_string(rdpSettings* settings, FreeRDP_Settings_Keys_String id,
                                    const char* separator, const char* param)
{
	const char* old = freerdp_settings_get_string(settings, id);

	size_t len = 0;
	char* str = NULL;

	if (!old)
		winpr_asprintf(&str, &len, "%s", param);
	else if (!separator)
		winpr_asprintf(&str, &len, "%s%s", old, param);
	else
		winpr_asprintf(&str, &len, "%s%s%s", old, separator, param);

	const BOOL rc = freerdp_settings_set_string_len(settings, id, str, len);
	free(str);
	return rc;
}

BOOL freerdp_settings_are_valid(const rdpSettings* settings)
{
	return settings != NULL;
}

/* Function to sort rdpMonitor arrays:
 * 1. first element is primary monitor
 * 2. all others are sorted by coordinates of x/y
 */
static int sort_monitor_fn(const void* pva, const void* pvb)
{
	const rdpMonitor* a = pva;
	const rdpMonitor* b = pvb;
	WINPR_ASSERT(a);
	WINPR_ASSERT(b);
	if (a->is_primary && b->is_primary)
		return 0;
	if (a->is_primary)
		return -1;
	if (b->is_primary)
		return 1;

	if (a->x != b->x)
		return a->x - b->x;
	if (a->y != b->y)
		return a->y - b->y;
	return 0;
}

BOOL freerdp_settings_set_monitor_def_array_sorted(rdpSettings* settings,
                                                   const rdpMonitor* monitors, size_t count)
{
	WINPR_ASSERT(monitors || (count == 0));
	if (count == 0)
	{
		if (!freerdp_settings_set_int32(settings, FreeRDP_MonitorLocalShiftX, 0))
			return FALSE;
		if (!freerdp_settings_set_int32(settings, FreeRDP_MonitorLocalShiftY, 0))
			return FALSE;
		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_MonitorDefArray, NULL, 0))
			return FALSE;
		return freerdp_settings_set_uint32(settings, FreeRDP_MonitorCount, 0);
	}

	// Find primary or alternatively the monitor at 0/0
	const rdpMonitor* primary = NULL;
	for (size_t x = 0; x < count; x++)
	{
		const rdpMonitor* cur = &monitors[x];
		if (cur->is_primary)
		{
			primary = cur;
			break;
		}
	}
	if (!primary)
	{
		for (size_t x = 0; x < count; x++)
		{
			const rdpMonitor* cur = &monitors[x];
			if ((cur->x == 0) && (cur->y == 0))
			{
				primary = cur;
				break;
			}
		}
	}

	if (!primary)
	{
		WLog_ERR(TAG, "Could not find primary monitor, aborting");
		return FALSE;
	}

	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_MonitorDefArray, NULL, count))
		return FALSE;
	rdpMonitor* sorted = freerdp_settings_get_pointer_writable(settings, FreeRDP_MonitorDefArray);
	WINPR_ASSERT(sorted);

	size_t sortpos = 0;

	/* Set primary. Ensure left/top is at 0/0 and flags contains MONITOR_PRIMARY */
	sorted[sortpos] = *primary;
	sorted[sortpos].x = 0;
	sorted[sortpos].y = 0;
	sorted[sortpos].is_primary = TRUE;
	sortpos++;

	/* Set monitor shift to original layout */
	const INT32 offsetX = primary->x;
	const INT32 offsetY = primary->y;
	if (!freerdp_settings_set_int32(settings, FreeRDP_MonitorLocalShiftX, offsetX))
		return FALSE;
	if (!freerdp_settings_set_int32(settings, FreeRDP_MonitorLocalShiftY, offsetY))
		return FALSE;

	for (size_t x = 0; x < count; x++)
	{
		const rdpMonitor* cur = &monitors[x];
		if (cur == primary)
			continue;

		rdpMonitor m = monitors[x];
		m.x -= offsetX;
		m.y -= offsetY;
		sorted[sortpos++] = m;
	}

	// Sort remaining monitors by x/y ?
	qsort(sorted, count, sizeof(rdpMonitor), sort_monitor_fn);

	return freerdp_settings_set_uint32(settings, FreeRDP_MonitorCount,
	                                   WINPR_ASSERTING_INT_CAST(uint32_t, count));
}

static BOOL fill_array(WINPR_JSON* array, const void* data, size_t length)
{
	const BYTE* pdata = data;
	for (size_t x = 0; x < length; x++)
	{
		BYTE val = pdata[x];
		if (!WINPR_JSON_AddItemToArray(array, WINPR_JSON_CreateNumber(val)))
			return FALSE;
	}
	return TRUE;
}

static BOOL fill_uint32_array(WINPR_JSON* array, const uint32_t* data, size_t length)
{
	for (size_t x = 0; x < length; x++)
	{
		uint32_t val = data[x];
		if (!WINPR_JSON_AddItemToArray(array, WINPR_JSON_CreateNumber(val)))
			return FALSE;
	}
	return TRUE;
}

static WINPR_JSON* json_from_addin_item(const ADDIN_ARGV* val)
{
	WINPR_JSON* obj = WINPR_JSON_CreateObject();
	if (!obj)
		goto fail;

	if (val)
	{
		if (!WINPR_JSON_AddNumberToObject(obj, "argc", val->argc))
			goto fail;

		WINPR_JSON* array = WINPR_JSON_AddArrayToObject(obj, "argv");
		if (!array)
			goto fail;
		for (int x = 0; x < val->argc; x++)
		{
			const char* str = val->argv[x];

			WINPR_JSON* item = NULL;
			if (!str)
			{
				item = WINPR_JSON_CreateNull();
			}
			else
			{
				item = WINPR_JSON_CreateString(str);
			}

			if (!WINPR_JSON_AddItemToArray(array, item))
				goto fail;
		}
	}
	return obj;

fail:
	WINPR_JSON_Delete(obj);
	return NULL;
}

static BOOL json_from_addin_item_array(WINPR_JSON* json, const rdpSettings* settings,
                                       FreeRDP_Settings_Keys_Pointer key, size_t count)
{
	if (!json)
		return FALSE;

	for (uint32_t x = 0; x < count; x++)
	{
		const ADDIN_ARGV* cval = freerdp_settings_get_pointer_array(settings, key, x);
		if (!WINPR_JSON_AddItemToArray(json, json_from_addin_item(cval)))
			return FALSE;
	}
	return TRUE;
}

static BOOL add_string_or_null(WINPR_JSON* json, const char* key, const char* value)
{
	if (value)
		return WINPR_JSON_AddStringToObject(json, key, value) != NULL;

	(void)WINPR_JSON_AddNullToObject(json, key);
	return TRUE;
}

static WINPR_JSON* json_from_device_item(const RDPDR_DEVICE* val)
{
	WINPR_JSON* obj = WINPR_JSON_CreateObject();
	if (!obj)
		goto fail;

	if (val)
	{
		union
		{
			const RDPDR_DEVICE* base;
			const RDPDR_PARALLEL* parallel;
			const RDPDR_SERIAL* serial;
			const RDPDR_SMARTCARD* smartcard;
			const RDPDR_PRINTER* printer;
			const RDPDR_DRIVE* drive;
			const RDPDR_DEVICE* device;
		} device;

		device.base = val;

		if (!WINPR_JSON_AddNumberToObject(obj, "Id", val->Id))
			goto fail;
		if (!WINPR_JSON_AddNumberToObject(obj, "Type", val->Type))
			goto fail;
		if (!add_string_or_null(obj, "Name", val->Name))
			goto fail;
		switch (val->Type)
		{
			case RDPDR_DTYP_SERIAL:
				if (!add_string_or_null(obj, "Path", device.serial->Path))
					goto fail;
				if (!add_string_or_null(obj, "Driver", device.serial->Driver))
					goto fail;
				if (!add_string_or_null(obj, "Permissive", device.serial->Permissive))
					goto fail;
				break;
			case RDPDR_DTYP_PARALLEL:
				if (!add_string_or_null(obj, "Path", device.parallel->Path))
					goto fail;
				break;
			case RDPDR_DTYP_PRINT:
				if (!add_string_or_null(obj, "DriverName", device.printer->DriverName))
					goto fail;
				if (!WINPR_JSON_AddBoolToObject(obj, "IsDefault", device.printer->IsDefault))
					goto fail;
				break;
			case RDPDR_DTYP_FILESYSTEM:
				if (!add_string_or_null(obj, "Path", device.drive->Path))
					goto fail;
				if (!WINPR_JSON_AddBoolToObject(obj, "IsDefault", device.drive->automount))
					goto fail;
				break;
			case RDPDR_DTYP_SMARTCARD:
			default:
				break;
		}
	}
	return obj;

fail:
	WINPR_JSON_Delete(obj);
	return NULL;
}

static BOOL json_from_device_item_array(WINPR_JSON* json, const rdpSettings* settings,
                                        FreeRDP_Settings_Keys_Pointer key, size_t count)
{
	if (!json)
		return FALSE;

	for (uint32_t x = 0; x < count; x++)
	{
		const RDPDR_DEVICE* cval = freerdp_settings_get_pointer_array(settings, key, x);
		if (!WINPR_JSON_AddItemToArray(json, json_from_device_item(cval)))
			return FALSE;
	}
	return TRUE;
}

static BOOL string_array_to_json(WINPR_JSON* json, const rdpSettings* settings, uint32_t argc,
                                 FreeRDP_Settings_Keys_Pointer key)
{
	for (uint32_t x = 0; x < argc; x++)
	{
		const char* cval = freerdp_settings_get_pointer_array(settings, key, x);

		WINPR_JSON* item = NULL;
		if (!cval)
			item = WINPR_JSON_CreateNull();
		else
			item = WINPR_JSON_CreateString(cval);
		if (!WINPR_JSON_AddItemToArray(json, item))
			return FALSE;
	}
	return TRUE;
}

static BOOL wchar_to_json(WINPR_JSON* obj, const char* key, const WCHAR* wstr, size_t len)
{
	if (len == 0)
		return WINPR_JSON_AddStringToObject(obj, key, "") != NULL;

	const size_t slen = len * 6;
	char* str = calloc(1, slen);
	if (!str)
		return FALSE;

	WINPR_JSON* jstr = NULL;
	SSIZE_T rc = ConvertWCharNToUtf8(wstr, len, str, slen);
	if (rc >= 0)
		jstr = WINPR_JSON_AddStringToObject(obj, key, str);
	free(str);
	return jstr != NULL;
}

static BOOL wchar_from_json(WCHAR* wstr, size_t len, const WINPR_JSON* obj, const char* key)
{
	if (!obj || !WINPR_JSON_IsObject(obj))
		return FALSE;
	WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(obj, key);
	if (!item || !WINPR_JSON_IsString(item))
		return FALSE;

	const char* str = WINPR_JSON_GetStringValue(item);
	if (!str)
	{
		memset(wstr, 0, sizeof(WCHAR) * len);
		return TRUE;
	}

	SSIZE_T rc = ConvertUtf8ToWChar(str, wstr, len);
	return rc >= 0;
}

static int64_t int_from_json_item(const WINPR_JSON* item, int64_t min, int64_t max)
{
	if (!item || !WINPR_JSON_IsNumber(item))
	{
		errno = EINVAL;
		return 0;
	}

	const double val = WINPR_JSON_GetNumberValue(item);
	if (isinf(val) || isnan(val))
	{
		errno = ERANGE;
		return 0;
	}

	const int64_t ival = (int64_t)val;
	if ((ival < min) || (ival > max))
	{
		errno = ERANGE;
		return 0;
	}

	return ival;
}

static int64_t int_from_json(const WINPR_JSON* obj, const char* key, int64_t min, int64_t max)
{
	if (!obj || !WINPR_JSON_IsObject(obj))
	{
		errno = EINVAL;
		return 0;
	}
	WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(obj, key);
	return int_from_json_item(item, min, max);
}

static uint64_t uint_from_json_item(const WINPR_JSON* item, uint64_t max)
{
	if (!item || !WINPR_JSON_IsNumber(item))
	{
		errno = EINVAL;
		return 0;
	}

	const double val = WINPR_JSON_GetNumberValue(item);
	if (isinf(val) || isnan(val) || (val < 0.0))
	{
		errno = ERANGE;
		return 0;
	}

	const uint64_t uval = (uint64_t)val;
	if (uval > max)
	{
		errno = ERANGE;
		return 0;
	}
	return uval;
}

static uint64_t uint_from_json(const WINPR_JSON* obj, const char* key, uint64_t max)
{
	if (!obj || !WINPR_JSON_IsObject(obj))
	{
		errno = EINVAL;
		return 0;
	}

	WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(obj, key);
	return uint_from_json_item(item, max);
}

static WINPR_JSON* systemtime_to_json(WINPR_JSON* parent, const char* key, const SYSTEMTIME* st)
{
	WINPR_ASSERT(st);

	WINPR_JSON* obj = WINPR_JSON_AddObjectToObject(parent, key);
	if (!obj)
		return NULL;

	if (!WINPR_JSON_AddNumberToObject(obj, "wYear", st->wYear))
		goto fail;
	if (!WINPR_JSON_AddNumberToObject(obj, "wMonth", st->wMonth))
		goto fail;
	if (!WINPR_JSON_AddNumberToObject(obj, "wDayOfWeek", st->wDayOfWeek))
		goto fail;
	if (!WINPR_JSON_AddNumberToObject(obj, "wDay", st->wDay))
		goto fail;
	if (!WINPR_JSON_AddNumberToObject(obj, "wHour", st->wHour))
		goto fail;
	if (!WINPR_JSON_AddNumberToObject(obj, "wMinute", st->wMinute))
		goto fail;
	if (!WINPR_JSON_AddNumberToObject(obj, "wSecond", st->wSecond))
		goto fail;
	if (!WINPR_JSON_AddNumberToObject(obj, "wMilliseconds", st->wMilliseconds))
		goto fail;

	return obj;
fail:
	WINPR_JSON_Delete(obj);
	return NULL;
}

static BOOL systemtime_from_json(const WINPR_JSON* pobj, const char* key, SYSTEMTIME* st)
{
	WINPR_ASSERT(st);

	if (!pobj || !WINPR_JSON_IsObject(pobj))
		return FALSE;

	WINPR_JSON* obj = WINPR_JSON_GetObjectItemCaseSensitive(pobj, key);
	if (!obj || !WINPR_JSON_IsObject(obj))
		return FALSE;

	errno = 0;
	st->wYear = (uint16_t)uint_from_json(obj, "wYear", UINT16_MAX);
	st->wMonth = (uint16_t)uint_from_json(obj, "wMonth", UINT16_MAX);
	st->wDayOfWeek = (uint16_t)uint_from_json(obj, "wDayOfWeek", UINT16_MAX);
	st->wDay = (uint16_t)uint_from_json(obj, "wDay", UINT16_MAX);
	st->wHour = (uint16_t)uint_from_json(obj, "wHour", UINT16_MAX);
	st->wMinute = (uint16_t)uint_from_json(obj, "wMinute", UINT16_MAX);
	st->wSecond = (uint16_t)uint_from_json(obj, "wSecond", UINT16_MAX);
	st->wMilliseconds = (uint16_t)uint_from_json(obj, "wMilliseconds", UINT16_MAX);
	return errno == 0;
}

static BOOL ts_info_from_json(TIME_ZONE_INFORMATION* tz, const WINPR_JSON* json)
{
	WINPR_ASSERT(tz);

	if (!json || !WINPR_JSON_IsObject(json))
		return FALSE;

	errno = 0;
	tz->Bias = (int32_t)int_from_json(json, "Bias", INT32_MIN, INT32_MAX);
	tz->StandardBias = (int32_t)int_from_json(json, "StandardBias", INT32_MIN, INT32_MAX);
	tz->DaylightBias = (int32_t)int_from_json(json, "DaylightBias", INT32_MIN, INT32_MAX);
	if (errno != 0)
		return FALSE;

	if (!systemtime_from_json(json, "StandardDate", &tz->StandardDate))
		return FALSE;
	if (!systemtime_from_json(json, "DaylightDate", &tz->DaylightDate))
		return FALSE;

	if (!wchar_from_json(tz->StandardName, ARRAYSIZE(tz->StandardName), json, "StandardName"))
		return FALSE;
	if (!wchar_from_json(tz->DaylightName, ARRAYSIZE(tz->DaylightName), json, "DaylightName"))
		return FALSE;

	return TRUE;
}

static BOOL ts_info_array_from_json(rdpSettings* settings, FreeRDP_Settings_Keys_Pointer key,
                                    const WINPR_JSON* json)
{
	if (!json || !WINPR_JSON_IsArray(json))
		return FALSE;

	const size_t count = WINPR_JSON_GetArraySize(json);
	if (!freerdp_settings_set_pointer_len(settings, key, NULL, count))
		return FALSE;

	for (size_t x = 0; x < count; x++)
	{
		TIME_ZONE_INFORMATION* tz = freerdp_settings_get_pointer_array_writable(settings, key, x);
		WINPR_JSON* obj = WINPR_JSON_GetArrayItem(json, x);
		if (!ts_info_from_json(tz, obj))
			return FALSE;
	}
	return TRUE;
}

static BOOL tz_info_to_json(WINPR_JSON* json, const TIME_ZONE_INFORMATION* ptz)
{
	WINPR_JSON* obj = WINPR_JSON_CreateObject();
	if (!obj)
		return FALSE;
	if (!WINPR_JSON_AddItemToArray(json, obj))
		return FALSE;

	TIME_ZONE_INFORMATION tz = { 0 };
	if (ptz)
		tz = *ptz;

	if (!WINPR_JSON_AddNumberToObject(obj, "Bias", tz.Bias))
		return FALSE;

	if (!wchar_to_json(obj, "StandardName", tz.StandardName, ARRAYSIZE(tz.StandardName)))
		return FALSE;

	if (!systemtime_to_json(obj, "StandardDate", &tz.StandardDate))
		return FALSE;

	if (!WINPR_JSON_AddNumberToObject(obj, "StandardBias", tz.Bias))
		return FALSE;

	if (!wchar_to_json(obj, "DaylightName", tz.DaylightName, ARRAYSIZE(tz.DaylightName)))
		return FALSE;

	if (!systemtime_to_json(obj, "DaylightDate", &tz.DaylightDate))
		return FALSE;

	if (!WINPR_JSON_AddNumberToObject(obj, "DaylightBias", tz.Bias))
		return FALSE;
	return TRUE;
}

static BOOL glyph_cache_def_to_json(WINPR_JSON* json, const GLYPH_CACHE_DEFINITION* def)
{
	WINPR_ASSERT(def);
	if (!json || !WINPR_JSON_IsArray(json))
		return FALSE;

	WINPR_JSON* obj = WINPR_JSON_CreateObject();
	if (!obj)
		return FALSE;
	if (!WINPR_JSON_AddItemToArray(json, obj))
		return FALSE;

	if (!WINPR_JSON_AddNumberToObject(obj, "cacheEntries", def->cacheEntries))
		return FALSE;
	if (!WINPR_JSON_AddNumberToObject(obj, "cacheMaximumCellSize", def->cacheMaximumCellSize))
		return FALSE;
	return TRUE;
}

static BOOL glyph_cache_def_array_to_json(WINPR_JSON* json, const GLYPH_CACHE_DEFINITION* def,
                                          size_t count)
{
	if (!json || !WINPR_JSON_IsArray(json))
		return FALSE;

	for (size_t x = 0; x < count; x++)
	{
		if (!glyph_cache_def_to_json(json, &def[x]))
			return FALSE;
	}
	return TRUE;
}

static BOOL glyph_cache_def_from_json(GLYPH_CACHE_DEFINITION* def, const WINPR_JSON* json)
{
	WINPR_ASSERT(def);
	WINPR_ASSERT(json);

	if (!WINPR_JSON_IsObject(json))
		return FALSE;

	errno = 0;
	def->cacheEntries = (uint16_t)uint_from_json(json, "cacheEntries", UINT16_MAX);
	def->cacheMaximumCellSize = (uint16_t)uint_from_json(json, "cacheMaximumCellSize", UINT16_MAX);
	return errno == 0;
}

static BOOL glyph_cache_def_array_from_json(rdpSettings* settings, FreeRDP_Settings_Keys_Pointer id,
                                            const WINPR_JSON* json)
{
	if (!json || !WINPR_JSON_IsArray(json))
		return FALSE;

	const size_t count = WINPR_JSON_GetArraySize(json);
	if (!freerdp_settings_set_pointer_len(settings, id, NULL, count))
		return FALSE;

	for (size_t x = 0; x < count; x++)
	{
		GLYPH_CACHE_DEFINITION* cache =
		    freerdp_settings_get_pointer_array_writable(settings, id, x);
		WINPR_JSON* obj = WINPR_JSON_GetArrayItem(json, x);
		if (!glyph_cache_def_from_json(cache, obj))
			return FALSE;
	}
	return TRUE;
}

static BOOL bitmap_cache_v2_from_json(BITMAP_CACHE_V2_CELL_INFO* info, const WINPR_JSON* json)
{
	WINPR_ASSERT(info);

	if (!json || !WINPR_JSON_IsObject(json))
		return FALSE;

	errno = 0;
	info->numEntries = (uint32_t)uint_from_json(json, "numEntries", UINT32_MAX);
	if (errno != 0)
		return FALSE;

	WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(json, "persistent");
	if (!item || !WINPR_JSON_IsBool(item))
		return FALSE;

	info->persistent = WINPR_JSON_IsTrue(item);
	return TRUE;
}

static BOOL bitmap_cache_v2_array_from_json(rdpSettings* settings, FreeRDP_Settings_Keys_Pointer id,
                                            const WINPR_JSON* json)
{
	if (!json || !WINPR_JSON_IsArray(json))
		return FALSE;

	const size_t count = WINPR_JSON_GetArraySize(json);
	if (!freerdp_settings_set_pointer_len(settings, id, NULL, count))
		return FALSE;

	for (size_t x = 0; x < count; x++)
	{
		BITMAP_CACHE_V2_CELL_INFO* cache =
		    freerdp_settings_get_pointer_array_writable(settings, id, x);
		WINPR_JSON* obj = WINPR_JSON_GetArrayItem(json, x);
		if (!bitmap_cache_v2_from_json(cache, obj))
			return FALSE;
	}
	return TRUE;
}

static BOOL client_cookie_from_json(ARC_CS_PRIVATE_PACKET* cookie, const WINPR_JSON* json)
{
	WINPR_ASSERT(cookie);
	WINPR_ASSERT(json);

	if (!WINPR_JSON_IsObject(json))
		return FALSE;

	errno = 0;
	cookie->cbLen = (uint32_t)uint_from_json(json, "cbLen", UINT32_MAX);
	cookie->version = (uint32_t)uint_from_json(json, "version", UINT32_MAX);
	cookie->logonId = (uint32_t)uint_from_json(json, "logonId", UINT32_MAX);
	if (errno != 0)
		return FALSE;

	WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(json, "securityVerifier");
	if (!item || !WINPR_JSON_IsArray(item))
		return FALSE;

	const size_t len = WINPR_JSON_GetArraySize(item);
	if (len != ARRAYSIZE(cookie->securityVerifier))
		return FALSE;

	errno = 0;
	for (size_t x = 0; x < len; x++)
	{
		WINPR_JSON* citem = WINPR_JSON_GetArrayItem(item, x);
		const uint64_t val = uint_from_json_item(citem, UINT8_MAX);
		cookie->securityVerifier[x] = (uint8_t)val;
	}
	return errno == 0;
}

static BOOL client_cookie_array_from_json(rdpSettings* settings, FreeRDP_Settings_Keys_Pointer id,
                                          const WINPR_JSON* json)
{
	if (!json || !WINPR_JSON_IsArray(json))
		return FALSE;

	const size_t count = WINPR_JSON_GetArraySize(json);
	if (!freerdp_settings_set_pointer_len(settings, id, NULL, count))
		return FALSE;

	for (size_t x = 0; x < count; x++)
	{
		ARC_CS_PRIVATE_PACKET* cache = freerdp_settings_get_pointer_array_writable(settings, id, x);
		WINPR_JSON* obj = WINPR_JSON_GetArrayItem(json, x);
		if (!client_cookie_from_json(cache, obj))
			return FALSE;
	}
	return TRUE;
}

static BOOL server_cookie_from_json(ARC_SC_PRIVATE_PACKET* cookie, const WINPR_JSON* json)
{
	WINPR_ASSERT(cookie);

	if (!json || !WINPR_JSON_IsObject(json))
		return FALSE;

	errno = 0;
	cookie->cbLen = (uint32_t)uint_from_json(json, "cbLen", UINT32_MAX);
	cookie->version = (uint32_t)uint_from_json(json, "version", UINT32_MAX);
	cookie->logonId = (uint32_t)uint_from_json(json, "logonId", UINT32_MAX);
	if (errno != 0)
		return FALSE;

	WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(json, "arcRandomBits");
	if (!item || !WINPR_JSON_IsArray(item))
		return FALSE;

	const size_t len = WINPR_JSON_GetArraySize(item);
	if (len != ARRAYSIZE(cookie->arcRandomBits))
		return FALSE;

	errno = 0;
	for (size_t x = 0; x < len; x++)
	{
		WINPR_JSON* citem = WINPR_JSON_GetArrayItem(item, x);
		cookie->arcRandomBits[x] = (uint8_t)uint_from_json_item(citem, UINT8_MAX);
	}
	return errno == 0;
}

static BOOL server_cookie_array_from_json(rdpSettings* settings, FreeRDP_Settings_Keys_Pointer id,
                                          const WINPR_JSON* json)
{
	if (!json || !WINPR_JSON_IsArray(json))
		return FALSE;

	const size_t count = WINPR_JSON_GetArraySize(json);
	if (!freerdp_settings_set_pointer_len(settings, id, NULL, count))
		return FALSE;

	for (size_t x = 0; x < count; x++)
	{
		ARC_SC_PRIVATE_PACKET* cache = freerdp_settings_get_pointer_array_writable(settings, id, x);
		WINPR_JSON* obj = WINPR_JSON_GetArrayItem(json, x);
		if (!server_cookie_from_json(cache, obj))
			return FALSE;
	}
	return TRUE;
}

static BOOL channel_def_from_json(CHANNEL_DEF* cookie, const WINPR_JSON* json)
{
	WINPR_ASSERT(cookie);
	WINPR_ASSERT(json);

	if (!WINPR_JSON_IsObject(json))
		return FALSE;

	errno = 0;
	cookie->options = (uint32_t)uint_from_json(json, "options", UINT32_MAX);
	if (errno != 0)
		return FALSE;

	WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(json, "name");
	if (!item || !WINPR_JSON_IsString(item))
		return FALSE;

	const char* str = WINPR_JSON_GetStringValue(item);
	if (!str)
		memset(cookie->name, 0, sizeof(cookie->name));
	else
	{
		strncpy(cookie->name, str, strnlen(str, ARRAYSIZE(cookie->name)));
	}
	return TRUE;
}

static BOOL channel_def_array_from_json(rdpSettings* settings, FreeRDP_Settings_Keys_Pointer id,
                                        const WINPR_JSON* json)
{
	if (!json || !WINPR_JSON_IsArray(json))
		return FALSE;

	const size_t count = WINPR_JSON_GetArraySize(json);
	if (!freerdp_settings_set_pointer_len(settings, id, NULL, count))
		return FALSE;

	for (size_t x = 0; x < count; x++)
	{
		CHANNEL_DEF* cache = freerdp_settings_get_pointer_array_writable(settings, id, x);
		WINPR_JSON* obj = WINPR_JSON_GetArrayItem(json, x);
		if (!channel_def_from_json(cache, obj))
			return FALSE;
	}
	return TRUE;
}

static BOOL monitor_attributes_from_json(MONITOR_ATTRIBUTES* attributes, const WINPR_JSON* json)
{
	WINPR_ASSERT(attributes);
	if (!json || !WINPR_JSON_IsObject(json))
		return FALSE;

	WINPR_JSON* obj = WINPR_JSON_GetObjectItemCaseSensitive(json, "attributes");
	if (!obj || !WINPR_JSON_IsObject(obj))
		return FALSE;

	errno = 0;
	attributes->physicalWidth = (uint32_t)uint_from_json(obj, "physicalWidth", UINT32_MAX);
	attributes->physicalHeight = (uint32_t)uint_from_json(obj, "physicalHeight", UINT32_MAX);
	attributes->orientation = (uint32_t)uint_from_json(obj, "orientation", UINT32_MAX);
	attributes->desktopScaleFactor =
	    (uint32_t)uint_from_json(obj, "desktopScaleFactor", UINT32_MAX);
	attributes->deviceScaleFactor = (uint32_t)uint_from_json(obj, "deviceScaleFactor", UINT32_MAX);
	return errno == 0;
}

static BOOL monitor_def_from_json(rdpMonitor* monitor, const WINPR_JSON* json)
{
	WINPR_ASSERT(monitor);

	if (!json || !WINPR_JSON_IsObject(json))
		return FALSE;

	errno = 0;
	monitor->x = (int32_t)int_from_json(json, "x", INT32_MIN, INT32_MAX);
	monitor->y = (int32_t)int_from_json(json, "y", INT32_MIN, INT32_MAX);
	monitor->width = (int32_t)int_from_json(json, "width", 0, INT32_MAX);
	monitor->height = (int32_t)int_from_json(json, "height", 0, INT32_MAX);
	monitor->orig_screen = (uint32_t)uint_from_json(json, "orig_screen", UINT32_MAX);
	if (errno != 0)
		return FALSE;

	WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(json, "is_primary");
	if (!item)
		return FALSE;
	if (!WINPR_JSON_IsBool(item))
		return FALSE;
	monitor->is_primary = WINPR_JSON_IsTrue(item) ? 1 : 0;

	return monitor_attributes_from_json(&monitor->attributes, json);
}

static BOOL monitor_def_array_from_json(rdpSettings* settings, FreeRDP_Settings_Keys_Pointer id,
                                        const WINPR_JSON* json)
{
	if (!json || !WINPR_JSON_IsArray(json))
		return FALSE;

	const size_t count = WINPR_JSON_GetArraySize(json);
	if (!freerdp_settings_set_pointer_len(settings, id, NULL, count))
		return FALSE;

	for (size_t x = 0; x < count; x++)
	{
		rdpMonitor* cache = freerdp_settings_get_pointer_array_writable(settings, id, x);
		WINPR_JSON* obj = WINPR_JSON_GetArrayItem(json, x);
		if (!monitor_def_from_json(cache, obj))
			return FALSE;
	}
	return TRUE;
}

static BOOL client_cookie_to_json(WINPR_JSON* json, const ARC_CS_PRIVATE_PACKET* cs)
{
	WINPR_JSON* obj = WINPR_JSON_CreateObject();
	if (!obj)
		return FALSE;
	if (!WINPR_JSON_AddItemToArray(json, obj))
		return FALSE;

	if (!WINPR_JSON_AddNumberToObject(obj, "cbLen", cs->cbLen))
		return FALSE;
	if (!WINPR_JSON_AddNumberToObject(obj, "version", cs->version))
		return FALSE;
	if (!WINPR_JSON_AddNumberToObject(obj, "logonId", cs->logonId))
		return FALSE;
	WINPR_JSON* array = WINPR_JSON_AddArrayToObject(obj, "securityVerifier");
	if (!array)
		return FALSE;

	for (size_t x = 0; x < ARRAYSIZE(cs->securityVerifier); x++)
	{
		WINPR_JSON* item = WINPR_JSON_CreateNumber(cs->securityVerifier[x]);
		if (!item)
			return FALSE;
		if (!WINPR_JSON_AddItemToArray(array, item))
			return FALSE;
	}
	return TRUE;
}

static BOOL client_cookie_array_to_json(WINPR_JSON* json, const ARC_CS_PRIVATE_PACKET* cs,
                                        size_t count)
{
	for (size_t x = 0; x < count; x++)
	{
		if (!client_cookie_to_json(json, &cs[x]))
			return FALSE;
	}
	return TRUE;
}

static BOOL server_cookie_to_json(WINPR_JSON* json, const ARC_SC_PRIVATE_PACKET* cs)
{
	WINPR_JSON* obj = WINPR_JSON_CreateObject();
	if (!obj)
		return FALSE;
	if (!WINPR_JSON_AddItemToArray(json, obj))
		return FALSE;

	if (!WINPR_JSON_AddNumberToObject(obj, "cbLen", cs->cbLen))
		return FALSE;
	if (!WINPR_JSON_AddNumberToObject(obj, "version", cs->version))
		return FALSE;
	if (!WINPR_JSON_AddNumberToObject(obj, "logonId", cs->logonId))
		return FALSE;
	WINPR_JSON* array = WINPR_JSON_AddArrayToObject(obj, "arcRandomBits");
	if (!array)
		return FALSE;

	for (size_t x = 0; x < ARRAYSIZE(cs->arcRandomBits); x++)
	{
		WINPR_JSON* item = WINPR_JSON_CreateNumber(cs->arcRandomBits[x]);
		if (!item)
			return FALSE;
		if (!WINPR_JSON_AddItemToArray(array, item))
			return FALSE;
	}
	return TRUE;
}

static BOOL server_cookie_array_to_json(WINPR_JSON* json, const ARC_SC_PRIVATE_PACKET* cs,
                                        size_t count)
{
	for (size_t x = 0; x < count; x++)
	{
		if (!server_cookie_to_json(json, &cs[x]))
			return FALSE;
	}
	return TRUE;
}

static BOOL bitmap_cache_v2_to_json(WINPR_JSON* json, const BITMAP_CACHE_V2_CELL_INFO* info)
{
	WINPR_JSON* obj = WINPR_JSON_CreateObject();
	if (!obj)
		return FALSE;
	if (!WINPR_JSON_AddItemToArray(json, obj))
		return FALSE;

	if (!WINPR_JSON_AddNumberToObject(obj, "numEntries", info->numEntries))
		return FALSE;
	if (!WINPR_JSON_AddBoolToObject(obj, "persistent", info->persistent))
		return FALSE;
	return TRUE;
}

static BOOL bitmap_cache_v2_array_to_json(WINPR_JSON* json, const BITMAP_CACHE_V2_CELL_INFO* info,
                                          size_t count)
{
	for (size_t x = 0; x < count; x++)
	{
		if (!bitmap_cache_v2_to_json(json, &info[x]))
			return FALSE;
	}
	return TRUE;
}

static BOOL monitor_attributes_to_json(WINPR_JSON* pobj, const MONITOR_ATTRIBUTES* attributes)
{
	WINPR_ASSERT(attributes);
	WINPR_JSON* obj = WINPR_JSON_AddObjectToObject(pobj, "attributes");
	if (!obj)
		return FALSE;
	if (!WINPR_JSON_AddNumberToObject(obj, "physicalWidth", attributes->physicalWidth))
		return FALSE;
	if (!WINPR_JSON_AddNumberToObject(obj, "physicalHeight", attributes->physicalHeight))
		return FALSE;
	if (!WINPR_JSON_AddNumberToObject(obj, "orientation", attributes->orientation))
		return FALSE;
	if (!WINPR_JSON_AddNumberToObject(obj, "desktopScaleFactor", attributes->desktopScaleFactor))
		return FALSE;
	if (!WINPR_JSON_AddNumberToObject(obj, "deviceScaleFactor", attributes->deviceScaleFactor))
		return FALSE;
	return TRUE;
}

static BOOL monitor_def_to_json(WINPR_JSON* json, const rdpMonitor* monitor)
{
	WINPR_ASSERT(monitor);
	WINPR_JSON* obj = WINPR_JSON_CreateObject();
	if (!obj)
		return FALSE;
	if (!WINPR_JSON_AddItemToArray(json, obj))
		return FALSE;

	if (!WINPR_JSON_AddNumberToObject(obj, "x", monitor->x))
		return FALSE;
	if (!WINPR_JSON_AddNumberToObject(obj, "y", monitor->y))
		return FALSE;
	if (!WINPR_JSON_AddNumberToObject(obj, "width", monitor->width))
		return FALSE;
	if (!WINPR_JSON_AddNumberToObject(obj, "height", monitor->height))
		return FALSE;
	if (!WINPR_JSON_AddBoolToObject(obj, "is_primary", monitor->is_primary != 0))
		return FALSE;
	if (!WINPR_JSON_AddNumberToObject(obj, "orig_screen", monitor->orig_screen))
		return FALSE;
	return monitor_attributes_to_json(obj, &monitor->attributes);
}

static BOOL monitor_def_array_to_json(WINPR_JSON* json, const rdpMonitor* monitors, size_t count)
{
	for (size_t x = 0; x < count; x++)
	{
		if (!monitor_def_to_json(json, &monitors[x]))
			return FALSE;
	}
	return TRUE;
}

static BOOL channel_def_to_json(WINPR_JSON* json, const CHANNEL_DEF* channel)
{
	WINPR_ASSERT(channel);
	WINPR_JSON* obj = WINPR_JSON_CreateObject();
	if (!obj)
		return FALSE;
	if (!WINPR_JSON_AddItemToArray(json, obj))
		return FALSE;

	if (!WINPR_JSON_AddStringToObject(obj, "name", channel->name))
		return FALSE;
	if (!WINPR_JSON_AddNumberToObject(obj, "options", channel->options))
		return FALSE;
	return TRUE;
}

static BOOL channel_def_array_to_json(WINPR_JSON* json, const CHANNEL_DEF* channels, size_t count)
{
	for (size_t x = 0; x < count; x++)
	{
		if (!channel_def_to_json(json, &channels[x]))
			return FALSE;
	}
	return TRUE;
}

static BOOL serialize_pointer(const rdpSettings* settings, WINPR_JSON* json,
                              FreeRDP_Settings_Keys_Pointer id)
{
	const char* name = freerdp_settings_get_name_for_key(id);
	if (!name)
		return FALSE;

	WINPR_JSON* jval = WINPR_JSON_AddArrayToObject(json, name);
	if (!jval)
		return FALSE;

	const void* val = freerdp_settings_get_pointer(settings, id);
	if (!val)
		return TRUE;

	switch (id)
	{
		case FreeRDP_instance:
		{
			union
			{
				const void* v;
				uintptr_t u;
			} ptr;

			ptr.v = val;
			return fill_array(jval, &ptr.u, sizeof(ptr.u));
		}
		case FreeRDP_ServerRandom:
		{
			const uint32_t len = freerdp_settings_get_uint32(settings, FreeRDP_ServerRandomLength);
			return fill_array(jval, val, len);
		}
		case FreeRDP_ServerCertificate:
		{
			const uint32_t len =
			    freerdp_settings_get_uint32(settings, FreeRDP_ServerCertificateLength);
			return fill_array(jval, val, len);
		}
		case FreeRDP_ClientRandom:
		{
			const uint32_t len = freerdp_settings_get_uint32(settings, FreeRDP_ClientRandomLength);
			return fill_array(jval, val, len);
		}
		case FreeRDP_ServerLicenseProductIssuers:
		{
			const uint32_t len =
			    freerdp_settings_get_uint32(settings, FreeRDP_ServerLicenseProductIssuersCount);
			return string_array_to_json(jval, settings, len, FreeRDP_ServerLicenseProductIssuers);
		}
		case FreeRDP_RedirectionPassword:
		{
			const uint32_t len =
			    freerdp_settings_get_uint32(settings, FreeRDP_RedirectionPasswordLength);
			return fill_array(jval, val, len);
		}
		case FreeRDP_RedirectionGuid:
		{
			const uint32_t len =
			    freerdp_settings_get_uint32(settings, FreeRDP_RedirectionGuidLength);
			return fill_array(jval, val, len);
		}
		case FreeRDP_LoadBalanceInfo:
		{
			const uint32_t len =
			    freerdp_settings_get_uint32(settings, FreeRDP_LoadBalanceInfoLength);
			return fill_array(jval, val, len);
		}
		case FreeRDP_ClientTimeZone:
		{
			return tz_info_to_json(jval, val);
		}
		case FreeRDP_RedirectionTsvUrl:
		{
			const uint32_t len =
			    freerdp_settings_get_uint32(settings, FreeRDP_RedirectionTsvUrlLength);
			return fill_array(jval, val, len);
		}
		case FreeRDP_GlyphCache:
		{
			return glyph_cache_def_array_to_json(jval, val, val ? 10 : 0);
		}
		case FreeRDP_FragCache:
		{
			return glyph_cache_def_array_to_json(jval, val, val ? 1 : 0);
		}
		case FreeRDP_BitmapCacheV2CellInfo:
		{
			const uint32_t len =
			    freerdp_settings_get_uint32(settings, FreeRDP_BitmapCacheV2NumCells);
			return bitmap_cache_v2_array_to_json(jval, val, len);
		}
		case FreeRDP_OrderSupport:
		{
			const uint32_t len = 32;
			return fill_array(jval, val, len);
		}
		case FreeRDP_ClientAutoReconnectCookie:
		{
			return client_cookie_array_to_json(jval, val, 1);
		}
		case FreeRDP_ServerAutoReconnectCookie:
		{
			return server_cookie_array_to_json(jval, val, 1);
		}
		case FreeRDP_Password51:
		{
			const uint32_t len = freerdp_settings_get_uint32(settings, FreeRDP_Password51Length);
			return fill_array(jval, val, len);
		}
		case FreeRDP_ReceivedCapabilities:
		{
			const uint32_t len =
			    freerdp_settings_get_uint32(settings, FreeRDP_ReceivedCapabilitiesSize);
			return fill_array(jval, val, len);
		}
		case FreeRDP_MonitorIds:
		{
			const uint32_t len = freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorIds);
			return fill_uint32_array(jval, val, len);
		}
		case FreeRDP_TargetNetPorts:
		{
			const uint32_t len =
			    freerdp_settings_get_uint32(settings, FreeRDP_TargetNetAddressCount);
			return fill_uint32_array(jval, val, len);
		}
		case FreeRDP_MonitorDefArray:
		{
			const uint32_t len = freerdp_settings_get_uint32(settings, FreeRDP_MonitorDefArraySize);
			return monitor_def_array_to_json(jval, val, len);
		}
		case FreeRDP_ChannelDefArray:
		{
			const uint32_t len = freerdp_settings_get_uint32(settings, FreeRDP_ChannelDefArraySize);
			return channel_def_array_to_json(jval, val, len);
		}
		case FreeRDP_ReceivedCapabilityDataSizes:
		{
			const uint32_t len =
			    freerdp_settings_get_uint32(settings, FreeRDP_ReceivedCapabilitiesSize);
			return fill_uint32_array(jval, val, len);
		}
		case FreeRDP_ReceivedCapabilityData:
		{
			const uint32_t len =
			    freerdp_settings_get_uint32(settings, FreeRDP_ReceivedCapabilitiesSize);
			const UINT32* pclen =
			    freerdp_settings_get_pointer(settings, FreeRDP_ReceivedCapabilityDataSizes);
			if (!pclen)
				return FALSE;

			for (uint32_t x = 0; x < len; x++)
			{
				const char* cval = freerdp_settings_get_pointer_array(settings, id, x);

				WINPR_JSON* item = WINPR_JSON_CreateArray();
				if (!item)
					return FALSE;
				if (!WINPR_JSON_AddItemToArray(jval, item))
					return FALSE;
				if (!fill_array(item, cval, pclen[x]))
					return FALSE;
			}
			return TRUE;
		}
		case FreeRDP_TargetNetAddresses:
		{
			const uint32_t len =
			    freerdp_settings_get_uint32(settings, FreeRDP_TargetNetAddressCount);
			return string_array_to_json(jval, settings, len, id);
		}
		case FreeRDP_RedirectionTargetCertificate:
		case FreeRDP_RdpServerCertificate:
		{
			WINPR_JSON* item = NULL;
			size_t len = 0;
			char* pem = freerdp_certificate_get_pem(val, &len);
			if (pem)
				item = WINPR_JSON_CreateString(pem);
			else if (val)
				item = WINPR_JSON_CreateString("");
			else
				item = WINPR_JSON_CreateNull();
			free(pem);
			if (!item)
				return TRUE;

			return WINPR_JSON_AddItemToArray(jval, item);
		}
		case FreeRDP_RdpServerRsaKey:
		{
			WINPR_JSON* item = NULL;
			size_t len = 0;
			char* pem = freerdp_key_get_pem(val, &len, NULL);
			if (pem)
				item = WINPR_JSON_CreateString(pem);
			free(pem);
			if (!item)
				return TRUE;

			return WINPR_JSON_AddItemToArray(jval, item);
		}
		case FreeRDP_DeviceArray:
		{
			const uint32_t len = freerdp_settings_get_uint32(settings, FreeRDP_DeviceArraySize);
			return json_from_device_item_array(jval, settings, id, len);
		}
		case FreeRDP_StaticChannelArray:
		{
			const uint32_t len =
			    freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelArraySize);
			return json_from_addin_item_array(jval, settings, id, len);
		}
		case FreeRDP_DynamicChannelArray:
		{
			const uint32_t len =
			    freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelArraySize);
			return json_from_addin_item_array(jval, settings, id, len);
		}
		case FreeRDP_POINTER_UNUSED:
		default:
			return FALSE;
	}
}

char* freerdp_settings_serialize(const rdpSettings* settings, BOOL pretty, size_t* plength)
{
	char* str = NULL;

	if (plength)
		*plength = 0;

	if (!settings)
		return NULL;

	WINPR_JSON* json = WINPR_JSON_CreateObject();
	if (!json)
		return NULL;

	WINPR_JSON* jbool = WINPR_JSON_AddObjectToObject(
	    json, freerdp_settings_get_type_name_for_type(RDP_SETTINGS_TYPE_BOOL));
	WINPR_JSON* juint16 = WINPR_JSON_AddObjectToObject(
	    json, freerdp_settings_get_type_name_for_type(RDP_SETTINGS_TYPE_UINT16));
	WINPR_JSON* jint16 = WINPR_JSON_AddObjectToObject(
	    json, freerdp_settings_get_type_name_for_type(RDP_SETTINGS_TYPE_INT16));
	WINPR_JSON* juint32 = WINPR_JSON_AddObjectToObject(
	    json, freerdp_settings_get_type_name_for_type(RDP_SETTINGS_TYPE_UINT32));
	WINPR_JSON* jint32 = WINPR_JSON_AddObjectToObject(
	    json, freerdp_settings_get_type_name_for_type(RDP_SETTINGS_TYPE_INT32));
	WINPR_JSON* juint64 = WINPR_JSON_AddObjectToObject(
	    json, freerdp_settings_get_type_name_for_type(RDP_SETTINGS_TYPE_UINT64));
	WINPR_JSON* jint64 = WINPR_JSON_AddObjectToObject(
	    json, freerdp_settings_get_type_name_for_type(RDP_SETTINGS_TYPE_INT64));
	WINPR_JSON* jstring = WINPR_JSON_AddObjectToObject(
	    json, freerdp_settings_get_type_name_for_type(RDP_SETTINGS_TYPE_STRING));
	WINPR_JSON* jpointer = WINPR_JSON_AddObjectToObject(
	    json, freerdp_settings_get_type_name_for_type(RDP_SETTINGS_TYPE_POINTER));
	if (!jbool || !juint16 || !jint16 || !juint32 || !jint32 || !juint64 || !jint64 || !jstring ||
	    !jpointer)
		goto fail;

	for (int x = 0; x < FreeRDP_Settings_StableAPI_MAX; x++)
	{
		union
		{

			int s;
			FreeRDP_Settings_Keys_Bool b;
			FreeRDP_Settings_Keys_Int16 i16;
			FreeRDP_Settings_Keys_UInt16 u16;
			FreeRDP_Settings_Keys_Int32 i32;
			FreeRDP_Settings_Keys_UInt32 u32;
			FreeRDP_Settings_Keys_Int64 i64;
			FreeRDP_Settings_Keys_UInt64 u64;
			FreeRDP_Settings_Keys_String str;
			FreeRDP_Settings_Keys_Pointer ptr;
		} iter;
		iter.s = x;

		const char* name = freerdp_settings_get_name_for_key(iter.s);
		SSIZE_T type = freerdp_settings_get_type_for_key(iter.s);
		switch (type)
		{
			case RDP_SETTINGS_TYPE_BOOL:
			{
				const BOOL val = freerdp_settings_get_bool(settings, iter.b);
				if (!WINPR_JSON_AddBoolToObject(jbool, name, val))
					goto fail;
			}
			break;
			case RDP_SETTINGS_TYPE_UINT16:
			{
				const uint16_t val = freerdp_settings_get_uint16(settings, iter.u16);
				if (!WINPR_JSON_AddNumberToObject(juint16, name, (double)val))
					goto fail;
			}
			break;
			case RDP_SETTINGS_TYPE_INT16:
			{
				const int16_t val = freerdp_settings_get_int16(settings, iter.i16);
				if (!WINPR_JSON_AddNumberToObject(jint16, name, (double)val))
					goto fail;
			}
			break;
			case RDP_SETTINGS_TYPE_UINT32:
			{
				const uint32_t val = freerdp_settings_get_uint32(settings, iter.u32);
				if (!WINPR_JSON_AddNumberToObject(juint32, name, (double)val))
					goto fail;
			}
			break;
			case RDP_SETTINGS_TYPE_INT32:
			{
				const int32_t val = freerdp_settings_get_int32(settings, iter.i32);
				if (!WINPR_JSON_AddNumberToObject(jint32, name, (double)val))
					goto fail;
			}
			break;
			case RDP_SETTINGS_TYPE_UINT64:
			{
				const uint64_t val = freerdp_settings_get_uint64(settings, iter.u64);
				if (!WINPR_JSON_AddNumberToObject(juint64, name, (double)val))
					goto fail;
			}
			break;
			case RDP_SETTINGS_TYPE_INT64:
			{
				const int64_t val = freerdp_settings_get_int64(settings, iter.i64);
				if (!WINPR_JSON_AddNumberToObject(jint64, name, (double)val))
					goto fail;
			}
			break;
			case RDP_SETTINGS_TYPE_STRING:
			{
				const char* val = freerdp_settings_get_string(settings, iter.str);
				if (val)
				{
					if (!WINPR_JSON_AddStringToObject(jstring, name, val))
						goto fail;
				}
				else
				{
					(void)WINPR_JSON_AddNullToObject(jstring, name);
				}
			}
			break;
			case RDP_SETTINGS_TYPE_POINTER:
				if (!serialize_pointer(settings, jpointer, iter.ptr))
					goto fail;
				break;
			default:
				break;
		}
	}

	if (pretty)
		str = WINPR_JSON_Print(json);
	else
		str = WINPR_JSON_PrintUnformatted(json);

	if (!str)
		goto fail;
	if (plength)
		*plength = strlen(str);

fail:
	WINPR_JSON_Delete(json);
	return str;
}

static BOOL val_from_array(rdpSettings* settings, const WINPR_JSON* json,
                           FreeRDP_Settings_Keys_Pointer key, size_t esize)
{
	if (WINPR_JSON_IsNull(json))
		return freerdp_settings_set_pointer(settings, key, NULL);
	if (!WINPR_JSON_IsArray(json))
		return FALSE;

	size_t len = WINPR_JSON_GetArraySize(json);
	if (len == 0)
		return freerdp_settings_set_pointer(settings, key, NULL);

	size_t count = len / esize;
	if (count * esize != len)
		return FALSE;

	if (!freerdp_settings_set_pointer_len(settings, key, NULL, count))
		return FALSE;

	BYTE* data = freerdp_settings_get_pointer_writable(settings, key);
	if (!data)
		return FALSE;

	errno = 0;
	for (size_t x = 0; x < len; x++)
	{
		WINPR_JSON* val = WINPR_JSON_GetArrayItem(json, x);
		data[x] = (uint8_t)uint_from_json_item(val, UINT8_MAX);
	}

	return errno == 0;
}

static BOOL uintptr_from_array(rdpSettings* settings, const WINPR_JSON* json)
{
	FreeRDP_Settings_Keys_Pointer key = FreeRDP_instance;
	if (!json || !WINPR_JSON_IsArray(json))
		return FALSE;

	size_t len = WINPR_JSON_GetArraySize(json);
	if (len > sizeof(void*))
		return FALSE;

	if (len == 0)
		return freerdp_settings_set_pointer(settings, key, NULL);

	union
	{
		void* v;
		uint8_t u[sizeof(void*)];
	} ptr;

	errno = 0;
	for (size_t x = 0; x < len; x++)
	{
		WINPR_JSON* val = WINPR_JSON_GetArrayItem(json, x);
		ptr.u[x] = (uint8_t)uint_from_json_item(val, UINT8_MAX);
	}
	if (errno != 0)
		return FALSE;
	return freerdp_settings_set_pointer(settings, key, ptr.v);
}

static BOOL val_from_uint32_array(rdpSettings* settings, const WINPR_JSON* json,
                                  FreeRDP_Settings_Keys_Pointer key,
                                  FreeRDP_Settings_Keys_UInt32 keyId)
{
	if (WINPR_JSON_IsNull(json))
		return freerdp_settings_set_pointer(settings, key, NULL);
	if (!WINPR_JSON_IsArray(json))
		return FALSE;

	const size_t len = WINPR_JSON_GetArraySize(json);
	if ((FreeRDP_UINT32_UNUSED != keyId) && (freerdp_settings_get_uint32(settings, keyId) != len))
	{
		if (!freerdp_settings_set_pointer_len(settings, key, NULL, len))
			return FALSE;
	}

	errno = 0;
	for (size_t x = 0; x < len; x++)
	{
		UINT32* data = freerdp_settings_get_pointer_array_writable(settings, key, x);
		if (!data)
			return FALSE;

		WINPR_JSON* val = WINPR_JSON_GetArrayItem(json, x);
		data[0] = (uint32_t)uint_from_json_item(val, UINT32_MAX);
	}
	return errno == 0;
}

static BOOL caps_data_entry_from_json(rdpSettings* settings, size_t offset, const WINPR_JSON* json)
{
	if (!json || !WINPR_JSON_IsArray(json))
		return FALSE;

	const size_t size = WINPR_JSON_GetArraySize(json);
	if (size == 0)
	{
		return freerdp_settings_set_pointer_array(settings, FreeRDP_ReceivedCapabilityData, offset,
		                                          NULL);
	}

	uint8_t* data = calloc(size, sizeof(uint8_t));
	if (!data)
		return FALSE;

	if (!freerdp_settings_set_pointer_array(settings, FreeRDP_ReceivedCapabilityData, offset, data))
	{
		free(data);
		return FALSE;
	}

	errno = 0;
	for (size_t x = 0; x < size; x++)
	{
		WINPR_JSON* item = WINPR_JSON_GetArrayItem(json, x);
		data[x] = (uint8_t)uint_from_json_item(item, UINT8_MAX);
	}

	// NOLINTNEXTLINE(clang-analyzer-unix.Malloc)
	return errno == 0;
}

static BOOL caps_data_array_from_json(rdpSettings* settings, const WINPR_JSON* json)
{
	if (!json || !WINPR_JSON_IsArray(json))
		return FALSE;

	const size_t count = WINPR_JSON_GetArraySize(json);
	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_ReceivedCapabilityData, NULL, count))
		return FALSE;

	for (uint32_t x = 0; x < count; x++)
	{
		WINPR_JSON* array = WINPR_JSON_GetArrayItem(json, x);
		if (!caps_data_entry_from_json(settings, x, array))
			return FALSE;
	}
	return TRUE;
}

static BOOL str_array_from_json(rdpSettings* settings, const WINPR_JSON* json,
                                FreeRDP_Settings_Keys_Pointer key)
{
	if (WINPR_JSON_IsNull(json))
		return freerdp_settings_set_pointer_len(settings, key, NULL, 0);
	if (!WINPR_JSON_IsArray(json))
		return FALSE;

	size_t len = WINPR_JSON_GetArraySize(json);
	if (!freerdp_settings_set_pointer_len(settings, key, NULL, len))
		return FALSE;

	for (size_t x = 0; x < len; x++)
	{
		WINPR_JSON* cval = WINPR_JSON_GetArrayItem(json, x);
		if (!cval)
			return FALSE;
		if (!WINPR_JSON_IsString(cval))
			return FALSE;

		const char* val = WINPR_JSON_GetStringValue(cval);
		if (!freerdp_settings_set_pointer_array(settings, key, x, val))
			return FALSE;
	}
	return TRUE;
}

static BOOL addin_argv_from_json(rdpSettings* settings, const WINPR_JSON* json,
                                 FreeRDP_Settings_Keys_Pointer key)
{
	if (WINPR_JSON_IsNull(json))
		return freerdp_settings_set_pointer(settings, key, NULL);

	if (!WINPR_JSON_IsArray(json))
		return FALSE;

	size_t len = WINPR_JSON_GetArraySize(json);
	if (!freerdp_settings_set_pointer_len(settings, key, NULL, len))
		return FALSE;

	for (size_t x = 0; x < len; x++)
	{
		WINPR_JSON* val = WINPR_JSON_GetArrayItem(json, x);
		if (val && WINPR_JSON_IsObject(val))
		{
			WINPR_JSON* jargc = WINPR_JSON_GetObjectItemCaseSensitive(val, "argc");
			WINPR_JSON* array = WINPR_JSON_GetObjectItemCaseSensitive(val, "argv");
			if (!jargc || !array)
				continue;
			if (!WINPR_JSON_IsNumber(jargc) || !WINPR_JSON_IsArray(array))
				continue;

			const int argc = (int)int_from_json_item(jargc, INT32_MIN, INT32_MAX);
			if (errno != 0)
				return FALSE;
			const size_t jlen = WINPR_JSON_GetArraySize(array);
			if (jlen != (size_t)argc)
				return FALSE;
			if (jlen == 0)
				continue;

			const char** argv = (const char**)calloc(jlen, sizeof(char*));
			if (!argv)
				return FALSE;
			for (size_t y = 0; y < jlen; y++)
			{
				WINPR_JSON* item = WINPR_JSON_GetArrayItem(array, y);
				if (!item || !WINPR_JSON_IsString(item))
				{
					free((void*)argv);
					return FALSE;
				}
				argv[y] = WINPR_JSON_GetStringValue(item);
			}

			ADDIN_ARGV* cval = freerdp_addin_argv_new(jlen, argv);
			free((void*)argv);
			if (!cval)
				return FALSE;
			const BOOL rc = freerdp_settings_set_pointer_array(settings, key, x, cval);
			freerdp_addin_argv_free(cval);
			if (!rc)
				return FALSE;
		}
	}
	return TRUE;
}

static char* get_string(const WINPR_JSON* json, const char* key)
{
	WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(json, key);
	if (!item || !WINPR_JSON_IsString(item))
		return NULL;
	const char* str = WINPR_JSON_GetStringValue(item);
	return WINPR_CAST_CONST_PTR_AWAY(str, char*);
}

static BOOL get_bool(const WINPR_JSON* json, const char* key)
{
	WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(json, key);
	if (!item || !WINPR_JSON_IsBool(item))
		return FALSE;
	return WINPR_JSON_IsTrue(item);
}

static BOOL device_from_json_item(rdpSettings* settings, FreeRDP_Settings_Keys_Pointer key,
                                  size_t offset, const WINPR_JSON* val)
{
	if (!val || !WINPR_JSON_IsObject(val))
		return FALSE;

	union
	{
		RDPDR_DEVICE base;
		RDPDR_PARALLEL parallel;
		RDPDR_SERIAL serial;
		RDPDR_SMARTCARD smartcard;
		RDPDR_PRINTER printer;
		RDPDR_DRIVE drive;
		RDPDR_DEVICE device;
	} device;

	memset(&device, 0, sizeof(device));

	errno = 0;
	device.base.Id = (uint32_t)uint_from_json(val, "Id", UINT32_MAX);
	device.base.Type = (uint32_t)uint_from_json(val, "Type", UINT32_MAX);
	if (errno != 0)
		return FALSE;
	device.base.Name = get_string(val, "Name");
	if (!device.base.Name)
		return FALSE;

	switch (device.base.Type)
	{
		case RDPDR_DTYP_SERIAL:
			device.serial.Path = get_string(val, "Path");
			device.serial.Driver = get_string(val, "Driver");
			device.serial.Permissive = get_string(val, "Permissive");
			break;
		case RDPDR_DTYP_PARALLEL:
			device.parallel.Path = get_string(val, "Path");
			break;
		case RDPDR_DTYP_PRINT:
			device.printer.DriverName = get_string(val, "DriverName");
			device.printer.IsDefault = get_bool(val, "IsDefault");
			break;
		case RDPDR_DTYP_FILESYSTEM:
			device.drive.Path = get_string(val, "Path");
			device.drive.automount = get_bool(val, "automount");
			break;
		case RDPDR_DTYP_SMARTCARD:
		default:
			break;
	}
	return freerdp_settings_set_pointer_array(settings, key, offset, &device);
}

static BOOL device_array_from_json(rdpSettings* settings, const WINPR_JSON* json,
                                   FreeRDP_Settings_Keys_Pointer key)
{
	if (WINPR_JSON_IsNull(json))
		return freerdp_settings_set_pointer(settings, key, NULL);

	if (!WINPR_JSON_IsArray(json))
		return FALSE;

	size_t len = WINPR_JSON_GetArraySize(json);
	if (!freerdp_settings_set_pointer_len(settings, key, NULL, len))
		return FALSE;

	for (size_t x = 0; x < len; x++)
	{
		WINPR_JSON* val = WINPR_JSON_GetArrayItem(json, x);
		if (!device_from_json_item(settings, key, x, val))
			return FALSE;
	}
	return TRUE;
}

static const char* pem_from_json(const WINPR_JSON* jval, size_t* plen, BOOL* pvalid)
{
	WINPR_ASSERT(jval);
	WINPR_ASSERT(plen);
	WINPR_ASSERT(pvalid);

	*pvalid = FALSE;
	*plen = 0;

	if (WINPR_JSON_IsNull(jval))
		return NULL;

	size_t len = WINPR_JSON_GetArraySize(jval);
	if (len == 0)
	{
		*pvalid = TRUE;
		return NULL;
	}

	WINPR_JSON* item = WINPR_JSON_GetArrayItem(jval, 0);
	if (!item)
		return NULL;
	if (!WINPR_JSON_IsString(item))
		return NULL;

	*plen = len;
	*pvalid = TRUE;
	return WINPR_JSON_GetStringValue(item);
}

static BOOL deserialize_pointer(const WINPR_JSON* json, rdpSettings* settings,
                                FreeRDP_Settings_Keys_Pointer id)
{
	const char* name = freerdp_settings_get_name_for_key(id);
	if (!name)
		return FALSE;

	if (!WINPR_JSON_HasObjectItem(json, name))
		return FALSE;

	WINPR_JSON* jval = WINPR_JSON_GetObjectItemCaseSensitive(json, name);
	if (!WINPR_JSON_IsNull(jval) && !WINPR_JSON_IsArray(jval))
		return FALSE;

	switch (id)
	{
		case FreeRDP_instance:
			return uintptr_from_array(settings, jval);
		case FreeRDP_ServerRandom:
		case FreeRDP_ServerCertificate:
		case FreeRDP_ClientRandom:
		case FreeRDP_RedirectionPassword:
		case FreeRDP_RedirectionGuid:
		case FreeRDP_LoadBalanceInfo:
		case FreeRDP_RedirectionTsvUrl:
		case FreeRDP_OrderSupport:
		case FreeRDP_Password51:
			return val_from_array(settings, jval, id, 1);
		case FreeRDP_ReceivedCapabilities:
			return val_from_array(settings, jval, id, 1);
		case FreeRDP_ClientTimeZone:
			return ts_info_array_from_json(settings, id, jval);
		case FreeRDP_GlyphCache:
			return glyph_cache_def_array_from_json(settings, id, jval);
		case FreeRDP_FragCache:
			return glyph_cache_def_array_from_json(settings, id, jval);
		case FreeRDP_BitmapCacheV2CellInfo:
			return bitmap_cache_v2_array_from_json(settings, id, jval);
		case FreeRDP_ClientAutoReconnectCookie:
			return client_cookie_array_from_json(settings, id, jval);
		case FreeRDP_ServerAutoReconnectCookie:
			return server_cookie_array_from_json(settings, id, jval);
		case FreeRDP_MonitorDefArray:
			return monitor_def_array_from_json(settings, id, jval);
		case FreeRDP_ChannelDefArray:
			return channel_def_array_from_json(settings, id, jval);
		case FreeRDP_MonitorIds:
			return val_from_uint32_array(settings, jval, id, FreeRDP_NumMonitorIds);
		case FreeRDP_TargetNetPorts:
			return val_from_uint32_array(settings, jval, id, FreeRDP_TargetNetAddressCount);
		case FreeRDP_ServerLicenseProductIssuers:
		case FreeRDP_TargetNetAddresses:
			return str_array_from_json(settings, jval, id);
		case FreeRDP_ReceivedCapabilityDataSizes:
			return val_from_uint32_array(settings, jval, id, FreeRDP_ReceivedCapabilitiesSize);
		case FreeRDP_ReceivedCapabilityData:
			return caps_data_array_from_json(settings, jval);
		case FreeRDP_RedirectionTargetCertificate:
		case FreeRDP_RdpServerCertificate:
		{
			size_t len = 0;
			BOOL valid = FALSE;
			const char* pem = pem_from_json(jval, &len, &valid);
			if (!valid)
				return FALSE;
			if (!freerdp_settings_set_pointer_len(settings, id, NULL, len))
				return FALSE;

			rdpCertificate* cert = NULL;
			if (!pem)
				return TRUE;

			if (strnlen(pem, 2) == 0)
				cert = freerdp_certificate_new();
			else
				cert = freerdp_certificate_new_from_pem(pem);
			if (!cert)
				return FALSE;
			return freerdp_settings_set_pointer_len(settings, id, cert, 1);
		}
		case FreeRDP_RdpServerRsaKey:
		{
			size_t len = 0;
			BOOL valid = FALSE;
			const char* pem = pem_from_json(jval, &len, &valid);
			if (!valid)
				return FALSE;
			if (!freerdp_settings_set_pointer_len(settings, id, NULL, len))
				return FALSE;
			if (!pem)
				return TRUE;

			rdpPrivateKey* key = freerdp_key_new_from_pem_enc(pem, NULL);
			if (!key)
				return FALSE;
			return freerdp_settings_set_pointer_len(settings, id, key, 1);
		}
		case FreeRDP_DeviceArray:
			return device_array_from_json(settings, jval, id);
		case FreeRDP_StaticChannelArray:
		case FreeRDP_DynamicChannelArray:
			return addin_argv_from_json(settings, jval, id);
		case FreeRDP_POINTER_UNUSED:
		default:
			return TRUE;
	}
}

rdpSettings* freerdp_settings_deserialize(const char* jstr, size_t length)
{
	WINPR_JSON* json = WINPR_JSON_ParseWithLength(jstr, length);
	if (!json)
		return NULL;

	WINPR_JSON* jbool = WINPR_JSON_GetObjectItemCaseSensitive(
	    json, freerdp_settings_get_type_name_for_type(RDP_SETTINGS_TYPE_BOOL));
	WINPR_JSON* juint16 = WINPR_JSON_GetObjectItemCaseSensitive(
	    json, freerdp_settings_get_type_name_for_type(RDP_SETTINGS_TYPE_UINT16));
	WINPR_JSON* jint16 = WINPR_JSON_GetObjectItemCaseSensitive(
	    json, freerdp_settings_get_type_name_for_type(RDP_SETTINGS_TYPE_INT16));
	WINPR_JSON* juint32 = WINPR_JSON_GetObjectItemCaseSensitive(
	    json, freerdp_settings_get_type_name_for_type(RDP_SETTINGS_TYPE_UINT32));
	WINPR_JSON* jint32 = WINPR_JSON_GetObjectItemCaseSensitive(
	    json, freerdp_settings_get_type_name_for_type(RDP_SETTINGS_TYPE_INT32));
	WINPR_JSON* juint64 = WINPR_JSON_GetObjectItemCaseSensitive(
	    json, freerdp_settings_get_type_name_for_type(RDP_SETTINGS_TYPE_UINT64));
	WINPR_JSON* jint64 = WINPR_JSON_GetObjectItemCaseSensitive(
	    json, freerdp_settings_get_type_name_for_type(RDP_SETTINGS_TYPE_INT64));
	WINPR_JSON* jstring = WINPR_JSON_GetObjectItemCaseSensitive(
	    json, freerdp_settings_get_type_name_for_type(RDP_SETTINGS_TYPE_STRING));
	WINPR_JSON* jpointer = WINPR_JSON_GetObjectItemCaseSensitive(
	    json, freerdp_settings_get_type_name_for_type(RDP_SETTINGS_TYPE_POINTER));

	rdpSettings* settings = freerdp_settings_new(0);
	if (!settings)
		goto fail;
	if (!jbool || !juint16 || !jint16 || !juint32 || !jint32 || !juint64 || !jint64 || !jstring ||
	    !jpointer)
		goto fail;

	for (int x = 0; x < FreeRDP_Settings_StableAPI_MAX; x++)
	{
		union
		{

			int s;
			FreeRDP_Settings_Keys_Bool b;
			FreeRDP_Settings_Keys_Int16 i16;
			FreeRDP_Settings_Keys_UInt16 u16;
			FreeRDP_Settings_Keys_Int32 i32;
			FreeRDP_Settings_Keys_UInt32 u32;
			FreeRDP_Settings_Keys_Int64 i64;
			FreeRDP_Settings_Keys_UInt64 u64;
			FreeRDP_Settings_Keys_String str;
			FreeRDP_Settings_Keys_Pointer ptr;
		} iter;
		iter.s = x;

		SSIZE_T type = freerdp_settings_get_type_for_key(iter.s);
		switch (type)
		{
			case RDP_SETTINGS_TYPE_POINTER:
				if (!deserialize_pointer(jpointer, settings, iter.ptr))
					goto fail;
				break;
			default:
				break;
		}
	}

	for (int x = 0; x < FreeRDP_Settings_StableAPI_MAX; x++)
	{
		union
		{

			int s;
			FreeRDP_Settings_Keys_Bool b;
			FreeRDP_Settings_Keys_Int16 i16;
			FreeRDP_Settings_Keys_UInt16 u16;
			FreeRDP_Settings_Keys_Int32 i32;
			FreeRDP_Settings_Keys_UInt32 u32;
			FreeRDP_Settings_Keys_Int64 i64;
			FreeRDP_Settings_Keys_UInt64 u64;
			FreeRDP_Settings_Keys_String str;
			FreeRDP_Settings_Keys_Pointer ptr;
		} iter;
		iter.s = x;

		const char* name = freerdp_settings_get_name_for_key(iter.s);
		SSIZE_T type = freerdp_settings_get_type_for_key(iter.s);
		switch (type)
		{
			case RDP_SETTINGS_TYPE_BOOL:
			{
				WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(jbool, name);
				if (!item)
					goto fail;
				if (!WINPR_JSON_IsBool(item))
					goto fail;
				const BOOL val = WINPR_JSON_IsTrue(item);
				if (!freerdp_settings_set_bool(settings, iter.b, val))
					goto fail;
			}
			break;
			case RDP_SETTINGS_TYPE_UINT16:
			{
				WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(juint16, name);
				const uint16_t val = (uint16_t)uint_from_json_item(item, UINT16_MAX);
				if (errno != 0)
					goto fail;
				if (!freerdp_settings_set_uint16(settings, iter.u16, val))
					goto fail;
			}
			break;
			case RDP_SETTINGS_TYPE_INT16:
			{
				WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(jint16, name);
				const int16_t val = (int16_t)int_from_json_item(item, INT16_MIN, INT16_MAX);
				if (errno != 0)
					goto fail;
				if (!freerdp_settings_set_int16(settings, iter.i16, val))
					goto fail;
			}
			break;
			case RDP_SETTINGS_TYPE_UINT32:
			{
				WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(juint32, name);
				const uint32_t val = (uint32_t)uint_from_json_item(item, UINT32_MAX);
				if (errno != 0)
					goto fail;
				if (!freerdp_settings_set_uint32(settings, iter.u32, val))
					goto fail;
			}
			break;
			case RDP_SETTINGS_TYPE_INT32:
			{
				const int64_t val = int_from_json(jint32, name, INT32_MIN, INT32_MAX);
				if (errno != 0)
					goto fail;
				if (!freerdp_settings_set_int32(settings, iter.i32, (int32_t)val))
					goto fail;
			}
			break;
			case RDP_SETTINGS_TYPE_UINT64:
			{
				const uint64_t val = uint_from_json(juint64, name, UINT64_MAX);
				if (errno != 0)
					goto fail;
				if (!freerdp_settings_set_uint64(settings, iter.u64, val))
					goto fail;
			}
			break;
			case RDP_SETTINGS_TYPE_INT64:
			{
				WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(jint64, name);
				const int64_t val = int_from_json_item(item, INT64_MIN, INT64_MAX);
				if (errno != 0)
					goto fail;
				if (!freerdp_settings_set_int64(settings, iter.i64, val))
					goto fail;
			}
			break;
			case RDP_SETTINGS_TYPE_STRING:
			{
				const char* val = NULL;
				WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(jstring, name);
				if (item && !WINPR_JSON_IsNull(item))
				{
					if (!WINPR_JSON_IsString(item))
						goto fail;
					val = WINPR_JSON_GetStringValue(item);
					if (!val)
						goto fail;
				}
				if (!freerdp_settings_set_string(settings, iter.str, val))
					goto fail;
			}
			break;
			case RDP_SETTINGS_TYPE_POINTER:
			default:
				break;
		}
	}

	WINPR_JSON_Delete(json);
	return settings;

fail:
	freerdp_settings_free(settings);
	WINPR_JSON_Delete(json);
	return NULL;
}
