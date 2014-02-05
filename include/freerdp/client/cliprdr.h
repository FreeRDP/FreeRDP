/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Clipboard Virtual Channel Extension
 *
 * Copyright 2011 Vic Lee
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

#ifndef FREERDP_CHANNEL_CLIENT_CLIPRDR_H
#define FREERDP_CHANNEL_CLIENT_CLIPRDR_H

#include <freerdp/types.h>

#include <freerdp/message.h>
#include <freerdp/channels/cliprdr.h>

/**
 * Client Interface
 */

typedef struct _cliprdr_client_context CliprdrClientContext;

typedef int (*pcCliprdrServerCapabilities)(CliprdrClientContext* context, CLIPRDR_CAPABILITIES* capabilities);
typedef int (*pcCliprdrClientCapabilities)(CliprdrClientContext* context, CLIPRDR_CAPABILITIES* capabilities);
typedef int (*pcCliprdrMonitorReady)(CliprdrClientContext* context, CLIPRDR_MONITOR_READY* monitorReady);
typedef int (*pcCliprdrClientFormatList)(CliprdrClientContext* context, CLIPRDR_FORMAT_LIST* formatList);
typedef int (*pcCliprdrServerFormatList)(CliprdrClientContext* context, CLIPRDR_FORMAT_LIST* formatList);
typedef int (*pcCliprdrClientFormatListResponse)(CliprdrClientContext* context, CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse);
typedef int (*pcCliprdrServerFormatListResponse)(CliprdrClientContext* context, CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse);
typedef int (*pcCliprdrClientFormatDataRequest)(CliprdrClientContext* context, CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest);
typedef int (*pcCliprdrServerFormatDataRequest)(CliprdrClientContext* context, CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest);
typedef int (*pcCliprdrClientFormatDataResponse)(CliprdrClientContext* context, CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse);
typedef int (*pcCliprdrServerFormatDataResponse)(CliprdrClientContext* context, CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse);

struct _cliprdr_client_context
{
	void* handle;
	void* custom;

	pcCliprdrServerCapabilities ServerCapabilities;
	pcCliprdrClientCapabilities ClientCapabilities;
	pcCliprdrMonitorReady MonitorReady;
	pcCliprdrClientFormatList ClientFormatList;
	pcCliprdrServerFormatList ServerFormatList;
	pcCliprdrClientFormatListResponse ClientFormatListResponse;
	pcCliprdrServerFormatListResponse ServerFormatListResponse;
	pcCliprdrClientFormatDataRequest ClientFormatDataRequest;
	pcCliprdrServerFormatDataRequest ServerFormatDataRequest;
	pcCliprdrClientFormatDataResponse ClientFormatDataResponse;
	pcCliprdrServerFormatDataResponse ServerFormatDataResponse;
};

struct _CLIPRDR_FORMAT_NAME
{
	UINT32 id;
	char* name;
	int length;
};
typedef struct _CLIPRDR_FORMAT_NAME CLIPRDR_FORMAT_NAME;

/**
 * Clipboard Events
 */

struct _RDP_CB_CLIP_CAPS
{
	wMessage event;
	UINT32 capabilities;
};
typedef struct _RDP_CB_CLIP_CAPS RDP_CB_CLIP_CAPS;

struct _RDP_CB_MONITOR_READY_EVENT
{
	wMessage event;
	UINT32 capabilities;
};
typedef struct _RDP_CB_MONITOR_READY_EVENT RDP_CB_MONITOR_READY_EVENT;

struct _RDP_CB_FORMAT_LIST_EVENT
{
	wMessage event;
	UINT32* formats;
	UINT16 num_formats;
	BYTE* raw_format_data;
	UINT32 raw_format_data_size;
	BOOL raw_format_unicode;
};
typedef struct _RDP_CB_FORMAT_LIST_EVENT RDP_CB_FORMAT_LIST_EVENT;

struct _RDP_CB_DATA_REQUEST_EVENT
{
	wMessage event;
	UINT32 format;
};
typedef struct _RDP_CB_DATA_REQUEST_EVENT RDP_CB_DATA_REQUEST_EVENT;

struct _RDP_CB_DATA_RESPONSE_EVENT
{
	wMessage event;
	BYTE* data;
	UINT32 size;
};
typedef struct _RDP_CB_DATA_RESPONSE_EVENT RDP_CB_DATA_RESPONSE_EVENT;

#endif /* FREERDP_CHANNEL_CLIENT_CLIPRDR_H */
