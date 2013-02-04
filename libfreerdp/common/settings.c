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
		settings->DeviceArray = (RDPDR_DEVICE**) realloc(settings->DeviceArray, settings->DeviceArraySize);
	}

	settings->DeviceArray[settings->DeviceCount++] = device;
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
				realloc(settings->StaticChannelArray, settings->StaticChannelArraySize);
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
				realloc(settings->DynamicChannelArray, settings->DynamicChannelArraySize);
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
