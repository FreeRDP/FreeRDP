/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Clipboard Virtual Channel Types
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

/**
 * Client Interface
 */

typedef struct _cliprdr_client_context CliprdrClientContext;

typedef int (*pcCliprdrMonitorReady)(CliprdrClientContext* context);
typedef int (*pcCliprdrFormatList)(CliprdrClientContext* context);
typedef int (*pcCliprdrDataRequest)(CliprdrClientContext* context);
typedef int (*pcCliprdrDataResponse)(CliprdrClientContext* context);

struct _cliprdr_client_context
{
	pcCliprdrMonitorReady MonitorReady;
	pcCliprdrFormatList FormatList;
	pcCliprdrDataRequest DataRequest;
	pcCliprdrDataResponse DataResponse;
};

/**
 * Clipboard Formats
 */

#define CB_FORMAT_RAW			0x0000
#define CB_FORMAT_TEXT			0x0001
#define CB_FORMAT_DIB			0x0008
#define CB_FORMAT_UNICODETEXT		0x000D
#define CB_FORMAT_HTML			0xD010
#define CB_FORMAT_PNG			0xD011
#define CB_FORMAT_JPEG			0xD012
#define CB_FORMAT_GIF			0xD013

/**
 * Clipboard Events
 */
typedef wMessage RDP_CB_MONITOR_READY_EVENT;

struct _RDP_CB_FORMAT_LIST_EVENT
{
	wMessage event;
	UINT32* formats;
	UINT16 num_formats;
	BYTE* raw_format_data;
	UINT32 raw_format_data_size;
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
