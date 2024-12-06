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

#include <winpr/crt.h>
#include <winpr/assert.h>

#include "../core/settings.h"
#include "../core/capabilities.h"

#include <freerdp/crypto/certificate.h>
#include <freerdp/settings.h>
#include <freerdp/freerdp.h>
#include <freerdp/log.h>

#define TAG FREERDP_TAG("common")

BOOL freerdp_addin_argv_add_argument_ex(ADDIN_ARGV* args, const char* argument, size_t len)
{
	char* str = NULL;
	char** new_argv = NULL;

	if (!args || !argument)
		return FALSE;

	if (len == 0)
		len = strlen(argument);

	new_argv = (char**)realloc((void*)args->argv, sizeof(char*) * (args->argc + 1));

	if (!new_argv)
		return FALSE;

	args->argv = new_argv;

	str = calloc(len + 1, sizeof(char));
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
			memmove_s((void*)&args->argv[x], (args->argc - x) * sizeof(char*),
			          (void*)&args->argv[x + 1], (args->argc - x - 1) * sizeof(char*));
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
			if (strncmp(args->argv[i], option, p - args->argv[i]) == 0)
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
		UINT32 new_size = old * 2;
		RDPDR_DEVICE** new_array = NULL;

		if (new_size == 0)
			new_size = count * 2;

		new_array =
		    (RDPDR_DEVICE**)realloc((void*)settings->DeviceArray, new_size * sizeof(RDPDR_DEVICE*));

		if (!new_array)
			return FALSE;

		settings->DeviceArray = new_array;
		memset((void*)&settings->DeviceArray[old], 0, (new_size - old) * sizeof(RDPDR_DEVICE*));

		if (!freerdp_settings_set_uint32(settings, FreeRDP_DeviceArraySize, new_size))
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
		count = 1;
		args[0] = device->Name;
	}

	switch (device->Type)
	{
		case RDPDR_DTYP_FILESYSTEM:
			if (src.drive->Path)
			{
				args[1] = src.drive->Path;
				count = 2;
			}
			break;

		case RDPDR_DTYP_PRINT:
			if (src.printer->DriverName)
			{
				args[1] = src.printer->DriverName;
				count = 2;
			}
			break;

		case RDPDR_DTYP_SMARTCARD:
			break;

		case RDPDR_DTYP_SERIAL:
			if (src.serial->Path)
			{
				args[1] = src.serial->Path;
				count = 2;
			}

			if (src.serial->Driver)
			{
				args[2] = src.serial->Driver;
				count = 3;
			}

			if (src.serial->Permissive)
			{
				args[3] = src.serial->Permissive;
				count = 4;
			}
			break;

		case RDPDR_DTYP_PARALLEL:
			if (src.parallel->Path)
			{
				args[1] = src.parallel->Path;
				count = 2;
			}
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
				const size_t rem = settings->StaticChannelArraySize - count + 1;
				memmove_s((void*)&settings->StaticChannelArray[x],
				          (count - x) * sizeof(ADDIN_ARGV*),
				          (void*)&settings->StaticChannelArray[x + 1],
				          (count - x - 1) * sizeof(ADDIN_ARGV*));
				memset((void*)&settings->StaticChannelArray[count - 1], 0,
				       sizeof(ADDIN_ARGV*) * rem);

				freerdp_addin_argv_free(cur);
				return freerdp_settings_set_uint32(settings, FreeRDP_StaticChannelCount, count - 1);
			}
		}
	}
	{
		const size_t rem = settings->StaticChannelArraySize - count;
		memset((void*)&settings->StaticChannelArray[count], 0, sizeof(ADDIN_ARGV*) * rem);
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
		UINT32 new_size = oldSize * 2ul;
		ADDIN_ARGV** new_array = NULL;
		if (new_size == 0)
			new_size = count * 2ul;

		new_array = (ADDIN_ARGV**)realloc((void*)settings->StaticChannelArray,
		                                  new_size * sizeof(ADDIN_ARGV*));

		if (!new_array)
			return FALSE;

		settings->StaticChannelArray = new_array;
		{
			const size_t rem = new_size - oldSize;
			memset((void*)&settings->StaticChannelArray[oldSize], 0, sizeof(ADDIN_ARGV*) * rem);
		}
		if (!freerdp_settings_set_uint32(settings, FreeRDP_StaticChannelArraySize, new_size))
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
				const size_t rem = settings->DynamicChannelArraySize - count + 1;
				memmove_s((void*)&settings->DynamicChannelArray[x],
				          (count - x) * sizeof(ADDIN_ARGV*),
				          (void*)&settings->DynamicChannelArray[x + 1],
				          (count - x - 1) * sizeof(ADDIN_ARGV*));
				memset((void*)&settings->DynamicChannelArray[count - 1], 0,
				       sizeof(ADDIN_ARGV*) * rem);

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
		ADDIN_ARGV** new_array = NULL;
		UINT32 size = oldSize * 2;
		if (size == 0)
			size = count * 2;

		new_array =
		    (ADDIN_ARGV**)realloc((void*)settings->DynamicChannelArray, sizeof(ADDIN_ARGV*) * size);

		if (!new_array)
			return FALSE;

		settings->DynamicChannelArray = new_array;
		{
			const size_t rem = size - oldSize;
			memset((void*)&settings->DynamicChannelArray[oldSize], 0, sizeof(ADDIN_ARGV*) * rem);
		}
		if (!freerdp_settings_set_uint32(settings, FreeRDP_DynamicChannelArraySize, size))
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
	return freerdp_addin_argv_new(args->argc, cnv.cc);
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

void freerdp_capability_buffer_free(rdpSettings* settings)
{
	WINPR_ASSERT(settings);

	if (settings->ReceivedCapabilityData)
	{
		for (UINT32 x = 0; x < settings->ReceivedCapabilitiesSize; x++)
		{
			free(settings->ReceivedCapabilityData[x]);
			settings->ReceivedCapabilityData[x] = NULL;
		}
	}
	settings->ReceivedCapabilitiesSize = 0;

	free(settings->ReceivedCapabilityDataSizes);
	settings->ReceivedCapabilityDataSizes = NULL;

	free((void*)settings->ReceivedCapabilityData);
	settings->ReceivedCapabilityData = NULL;
	free(settings->ReceivedCapabilities);
	settings->ReceivedCapabilities = NULL;
}

BOOL freerdp_capability_buffer_copy(rdpSettings* settings, const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	if (!freerdp_capability_buffer_allocate(settings, src->ReceivedCapabilitiesSize))
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

void freerdp_target_net_addresses_free(rdpSettings* settings)
{
	WINPR_ASSERT(settings);

	if (settings->TargetNetAddresses)
	{
		for (UINT32 index = 0; index < settings->TargetNetAddressCount; index++)
			free(settings->TargetNetAddresses[index]);
	}

	free((void*)settings->TargetNetAddresses);
	free(settings->TargetNetPorts);
	settings->TargetNetAddressCount = 0;
	settings->TargetNetAddresses = NULL;
	settings->TargetNetPorts = NULL;
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
			if ((data == NULL) && (len == 0))
			{
				freerdp_target_net_addresses_free(settings);
				return TRUE;
			}
			WLog_WARN(
			    TAG,
			    "[BUG] FreeRDP_TargetNetAddresses must not be resized from outside the library!");
			return FALSE;
		case FreeRDP_ServerLicenseProductIssuers:
			if (data == NULL)
				freerdp_server_license_issuers_free(settings);
			return freerdp_settings_set_pointer_len_(settings, FreeRDP_ServerLicenseProductIssuers,
			                                         FreeRDP_ServerLicenseProductIssuersCount, data,
			                                         len, sizeof(char*));
		case FreeRDP_TargetNetPorts:
			if ((data == NULL) && (len == 0))
			{
				freerdp_target_net_addresses_free(settings);
				return TRUE;
			}
			WLog_WARN(TAG,
			          "[BUG] FreeRDP_TargetNetPorts must not be resized from outside the library!");
			return FALSE;
		case FreeRDP_DeviceArray:
			if (data == NULL)
				freerdp_device_collection_free(settings);
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_DeviceArraySize, data,
			                                         len, sizeof(ADDIN_ARGV*));
		case FreeRDP_ChannelDefArray:
			if ((len > 0) && (len < CHANNEL_MAX_COUNT))
				WLog_WARN(TAG,
				          "FreeRDP_ChannelDefArray::len expected to be >= %" PRIu32
				          ", but have %" PRIu32,
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
			if (data == NULL)
				freerdp_capability_buffer_free(settings);
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_ReceivedCapabilitiesSize,
			                                         data, len, sizeof(BYTE*));
		case FreeRDP_ReceivedCapabilities:
			if (data == NULL)
				freerdp_capability_buffer_free(settings);
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_ReceivedCapabilitiesSize,
			                                         data, len, sizeof(char));
		case FreeRDP_OrderSupport:
			return freerdp_settings_set_pointer_len_(settings, id, FreeRDP_UINT32_UNUSED, data, len,
			                                         sizeof(char));

		case FreeRDP_MonitorIds:
			return freerdp_settings_set_pointer_len_(
			    settings, FreeRDP_MonitorIds, FreeRDP_NumMonitorIds, data, len, sizeof(UINT32));

		default:
			if ((data == NULL) && (len == 0))
			{
				freerdp_settings_set_pointer(settings, id, NULL);
			}
			else
				WLog_WARN(TAG, "Invalid id %" PRIuz, id);
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
			return settings->TargetNetAddresses[offset];
		case FreeRDP_TargetNetPorts:
			max = freerdp_settings_get_uint32(settings, FreeRDP_TargetNetAddressCount);
			if (offset >= max)
				goto fail;
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
		default:
			WLog_WARN(TAG, "Invalid id %s [%" PRIuz "]", freerdp_settings_get_name_for_key(id), id);
			return NULL;
	}

fail:
	WLog_WARN(TAG, "Invalid offset for %s [%" PRIuz "]: size=%" PRIuz ", offset=%" PRIuz,
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
			free(settings->TargetNetAddresses[offset]);
			settings->TargetNetAddresses[offset] = _strdup((const char*)data);
			return settings->TargetNetAddresses[offset] != NULL;
		case FreeRDP_TargetNetPorts:
			maxOffset = freerdp_settings_get_uint32(settings, FreeRDP_TargetNetAddressCount);
			if ((offset >= maxOffset) || !data)
				goto fail;
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
			settings->OrderSupport[offset] = *(const BOOL*)data;
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
			settings->ClientTimeZone[0] = *(const TIME_ZONE_INFORMATION*)data;
			return TRUE;

		default:
			WLog_WARN(TAG, "Invalid id %s [%" PRIuz "]", freerdp_settings_get_name_for_key(id), id);
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
		flags &= ~FREERDP_CODEC_REMOTEFX;
	}
	if (settings->NSCodec == FALSE)
	{
		flags &= ~FREERDP_CODEC_NSCODEC;
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
		if (!freerdp_settings_set_pointer_array(settings, FreeRDP_TargetNetAddresses, i,
		                                        addresses[i]))
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

	for (UINT32 x = 0; x < capsCount; x++)
	{
		if (capsFlags[x])
		{
			wStream buffer;
			wStream* sub = Stream_StaticConstInit(&buffer, capsData[x], capsSizes[x]);

			if (!rdp_read_capability_set(sub, (UINT16)x, settings, serverReceivedCaps))
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
