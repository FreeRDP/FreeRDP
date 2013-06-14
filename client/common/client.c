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

#include <freerdp/client/file.h>
#include <freerdp/client/cmdline.h>

freerdp* freerdp_client_get_instance(rdpContext* context)
{
	return context->instance;
}

BOOL freerdp_client_get_param_bool(rdpContext* context, int id)
{
	rdpSettings* settings = context->settings;
	return freerdp_get_param_bool(settings, id);
}

int freerdp_client_set_param_bool(rdpContext* context, int id, BOOL param)
{
	rdpSettings* settings = context->settings;
	return freerdp_set_param_bool(settings, id, param);
}

UINT32 freerdp_client_get_param_uint32(rdpContext* context, int id)
{
	rdpSettings* settings = context->settings;
	return freerdp_get_param_uint32(settings, id);
}

int freerdp_client_set_param_uint32(rdpContext* context, int id, UINT32 param)
{
	rdpSettings* settings = context->settings;
	return freerdp_set_param_uint32(settings, id, param);
}

UINT64 freerdp_client_get_param_uint64(rdpContext* context, int id)
{
	rdpSettings* settings = context->settings;
	return freerdp_get_param_uint64(settings, id);
}

int freerdp_client_set_param_uint64(rdpContext* context, int id, UINT64 param)
{
	rdpSettings* settings = context->settings;
	return freerdp_set_param_uint64(settings, id, param);
}

char* freerdp_client_get_param_string(rdpContext* context, int id)
{
	rdpSettings* settings = context->settings;
	return freerdp_get_param_string(settings, id);
}

int freerdp_client_set_param_string(rdpContext* context, int id, char* param)
{
	rdpSettings* settings = context->settings;
	return freerdp_set_param_string(settings, id, param);
}

/* Common API */

rdpContext* freerdp_client_context_new(RDP_CLIENT_ENTRY_POINTS* pEntryPoints)
{
	freerdp* instance;
	rdpContext* context;

	pEntryPoints->GlobalInit();

	instance = freerdp_new();
	instance->ContextSize = pEntryPoints->ContextSize;
	instance->ContextNew = pEntryPoints->ClientNew;
	instance->ContextFree = pEntryPoints->ClientFree;
	freerdp_context_new(instance);

	context = instance->context;

	context->client->pEntryPoints = (RDP_CLIENT_ENTRY_POINTS*) malloc(pEntryPoints->Size);
	CopyMemory(context->client->pEntryPoints, pEntryPoints, pEntryPoints->Size);

	return context;
}

void freerdp_client_context_free(rdpContext* context)
{
	freerdp* instance = context->instance;

	freerdp_context_free(instance);
	freerdp_free(instance);
}

int freerdp_client_start(rdpContext* context)
{
	rdpClient* client = context->client;
	return client->pEntryPoints->ClientStart(context);
}

int freerdp_client_stop(rdpContext* context)
{
	rdpClient* client = context->client;
	return client->pEntryPoints->ClientStop(context);
}

int freerdp_client_parse_command_line(rdpContext* context, int argc, char** argv)
{
	int status;
	rdpSettings* settings;

	context->argc = argc;
	context->argv = argv;

	if (context->argc < 1)
		return 0;

	if (!context->argv)
		return -1;

	settings = context->settings;

	status = freerdp_client_parse_command_line_arguments(context->argc, context->argv, settings);

	if (settings->ConnectionFile)
	{
		rdpFile* file = freerdp_client_rdp_file_new();
		freerdp_client_parse_rdp_file(file, settings->ConnectionFile);
		freerdp_client_populate_settings_from_rdp_file(file, settings);
		freerdp_client_rdp_file_free(file);
	}

	return status;
}

int freerdp_client_parse_connection_file(rdpContext* context, char* filename)
{
	rdpFile* file;

	file = freerdp_client_rdp_file_new();
	freerdp_client_parse_rdp_file(file, filename);
	freerdp_client_populate_settings_from_rdp_file(file, context->settings);
	freerdp_client_rdp_file_free(file);

	return 0;
}

int freerdp_client_parse_connection_file_buffer(rdpContext* context, BYTE* buffer, size_t size)
{
	rdpFile* file;

	file = freerdp_client_rdp_file_new();
	freerdp_client_parse_rdp_file_buffer(file, buffer, size);
	freerdp_client_populate_settings_from_rdp_file(file, context->settings);
	freerdp_client_rdp_file_free(file);

	return 0;
}
