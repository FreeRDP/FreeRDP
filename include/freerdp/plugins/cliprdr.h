/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#ifndef __CLIPRDR_PLUGIN
#define __CLIPRDR_PLUGIN

/**
 * Event Types
 */
enum RDP_EVENT_TYPE_CB
{
	RDP_EVENT_TYPE_CB_SYNC = 1,
	RDP_EVENT_TYPE_CB_FORMAT_LIST,
	RDP_EVENT_TYPE_CB_DATA_REQUEST,
	RDP_EVENT_TYPE_CB_DATA_RESPONSE
};

/**
 * Clipboard Formats
 */
enum RDP_CB_FORMAT
{
	CB_FORMAT_RAW = 0,
	CB_FORMAT_TEXT = 1,
	CB_FORMAT_DIB = 8,
	CB_FORMAT_UNICODETEXT = 13,
	CB_FORMAT_HTML = 0xD010,
	CB_FORMAT_PNG = 0xD011,
	CB_FORMAT_JPEG = 0xD012,
	CB_FORMAT_GIF = 0xD013
};

/**
 * Clipboard Events
 */
typedef RDP_EVENT RDP_CB_SYNC_EVENT;

struct _RDP_CB_FORMAT_LIST_EVENT
{
	RDP_EVENT event;
	uint32* formats;
	uint16 num_formats;
	uint8* raw_format_data;
	uint32 raw_format_data_size;
};
typedef struct _RDP_CB_FORMAT_LIST_EVENT RDP_CB_FORMAT_LIST_EVENT;

struct _RDP_CB_DATA_REQUEST_EVENT
{
	RDP_EVENT event;
	uint32 format;
};
typedef struct _RDP_CB_DATA_REQUEST_EVENT RDP_CB_DATA_REQUEST_EVENT;

struct _RDP_CB_DATA_RESPONSE_EVENT
{
	RDP_EVENT event;
	uint8* data;
	uint32 size;
};
typedef struct _RDP_CB_DATA_RESPONSE_EVENT RDP_CB_DATA_RESPONSE_EVENT;

#endif /* __CLIPRDR_PLUGIN */
