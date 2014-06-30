/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Client Common
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

#include <freerdp/client.h>

#include <freerdp/addin.h>
#include <freerdp/assistance.h>
#include <freerdp/client/file.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/channels.h>

int freerdp_client_common_new(freerdp* instance, rdpContext* context)
{
	RDP_CLIENT_ENTRY_POINTS* pEntryPoints = instance->pClientEntryPoints;
	return pEntryPoints->ClientNew(instance, context);
}

void freerdp_client_common_free(freerdp* instance, rdpContext* context)
{
	RDP_CLIENT_ENTRY_POINTS* pEntryPoints = instance->pClientEntryPoints;
	pEntryPoints->ClientFree(instance, context);
}

/* Common API */

rdpContext* freerdp_client_context_new(RDP_CLIENT_ENTRY_POINTS* pEntryPoints)
{
	freerdp* instance;
	rdpContext* context;

	pEntryPoints->GlobalInit();

	instance = freerdp_new();
	instance->settings = pEntryPoints->settings;
	instance->ContextSize = pEntryPoints->ContextSize;
	instance->ContextNew = freerdp_client_common_new;
	instance->ContextFree = freerdp_client_common_free;
	instance->pClientEntryPoints = (RDP_CLIENT_ENTRY_POINTS*) malloc(pEntryPoints->Size);
	CopyMemory(instance->pClientEntryPoints, pEntryPoints, pEntryPoints->Size);
	freerdp_context_new(instance);

	context = instance->context;
	context->instance = instance;
	context->settings = instance->settings;

	freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry, 0);

	return context;
}

void freerdp_client_context_free(rdpContext* context)
{
	freerdp* instance = context->instance;

	if (instance)
	{
		freerdp_context_free(instance);
		free(instance->pClientEntryPoints);
		freerdp_free(instance);
	}
}

int freerdp_client_start(rdpContext* context)
{
	RDP_CLIENT_ENTRY_POINTS* pEntryPoints = context->instance->pClientEntryPoints;
	return pEntryPoints->ClientStart(context);
}

int freerdp_client_stop(rdpContext* context)
{
	RDP_CLIENT_ENTRY_POINTS* pEntryPoints = context->instance->pClientEntryPoints;
	return pEntryPoints->ClientStop(context);
}

freerdp* freerdp_client_get_instance(rdpContext* context)
{
	return context->instance;
}

HANDLE freerdp_client_get_thread(rdpContext* context)
{
	return ((rdpClientContext*) context)->thread;
}

int freerdp_client_settings_parse_command_line(rdpSettings* settings, int argc, char** argv)
{
	int status;

	if (argc < 1)
		return 0;

	if (!argv)
		return -1;

	status = freerdp_client_settings_parse_command_line_arguments(settings, argc, argv);

	if (settings->ConnectionFile)
	{
		status = freerdp_client_settings_parse_connection_file(settings, settings->ConnectionFile);
	}

	if (settings->AssistanceFile)
	{
		status = freerdp_client_settings_parse_assistance_file(settings, settings->AssistanceFile);
	}

	return status;
}

int freerdp_client_settings_parse_connection_file(rdpSettings* settings, const char* filename)
{
	rdpFile* file;

	file = freerdp_client_rdp_file_new();
	freerdp_client_parse_rdp_file(file, filename);
	freerdp_client_populate_settings_from_rdp_file(file, settings);
	freerdp_client_rdp_file_free(file);

	return 0;
}

int freerdp_client_settings_parse_connection_file_buffer(rdpSettings* settings, const BYTE* buffer, size_t size)
{
	rdpFile* file;
	int status = -1;

	file = freerdp_client_rdp_file_new();

	if (freerdp_client_parse_rdp_file_buffer(file, buffer, size)
			&& freerdp_client_populate_settings_from_rdp_file(file, settings))
	{
		status = 0;
	}

	freerdp_client_rdp_file_free(file);

	return status;
}

int freerdp_client_settings_write_connection_file(const rdpSettings* settings, const char* filename, BOOL unicode)
{
	rdpFile* file;

	file = freerdp_client_rdp_file_new();

	if (!freerdp_client_populate_rdp_file_from_settings(file, settings))
		return -1;

	if (!freerdp_client_write_rdp_file(file, filename, unicode))
		return -1;

	freerdp_client_rdp_file_free(file);

	return 0;
}

int freerdp_client_settings_parse_assistance_file(rdpSettings* settings, const char* filename)
{
	int status;
	rdpAssistanceFile* file;

	file = freerdp_assistance_file_new();

	if (!file)
		return -1;

	status = freerdp_assistance_parse_file(file, filename);

	if (status < 0)
		return -1;

	status = freerdp_client_populate_settings_from_assistance_file(file, settings);

	if (status < 0)
		return -1;

	freerdp_assistance_file_free(file);

	return 0;
}
