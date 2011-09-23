/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Clipboard Virtual Channel
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/constants.h>
#include <freerdp/types.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/svc_plugin.h>
#include <freerdp/plugins/cliprdr.h>

#include "cliprdr_constants.h"
#include "cliprdr_main.h"
#include "cliprdr_format.h"

#define CFSTR_HTML      "H\0T\0M\0L\0 \0F\0o\0r\0m\0a\0t\0\0"
#define CFSTR_PNG       "P\0N\0G\0\0"
#define CFSTR_JPEG      "J\0F\0I\0F\0\0"
#define CFSTR_GIF       "G\0I\0F\0\0"

void cliprdr_process_format_list_event(cliprdrPlugin* cliprdr, RDP_CB_FORMAT_LIST_EVENT* cb_event)
{
	STREAM* s;
	int i;

	if (cb_event->raw_format_data)
	{
		s = cliprdr_packet_new(CB_FORMAT_LIST, 0, cb_event->raw_format_data_size);
		stream_write(s, cb_event->raw_format_data, cb_event->raw_format_data_size);
	}
	else
	{
		s = cliprdr_packet_new(CB_FORMAT_LIST, 0, 36 * cb_event->num_formats);

		for (i = 0; i < cb_event->num_formats; i++)
		{
			stream_write_uint32(s, cb_event->formats[i]);
			switch (cb_event->formats[i])
			{
				case CB_FORMAT_HTML:
					memcpy(stream_get_tail(s), CFSTR_HTML, sizeof(CFSTR_HTML));
					break;
				case CB_FORMAT_PNG:
					memcpy(stream_get_tail(s), CFSTR_PNG, sizeof(CFSTR_PNG));
					break;
				case CB_FORMAT_JPEG:
					memcpy(stream_get_tail(s), CFSTR_JPEG, sizeof(CFSTR_JPEG));
					break;
				case CB_FORMAT_GIF:
					memcpy(stream_get_tail(s), CFSTR_GIF, sizeof(CFSTR_GIF));
					break;
			}
			stream_seek(s, 32);
		}
	}

	cliprdr_packet_send(cliprdr, s);
}

static void cliprdr_send_format_list_response(cliprdrPlugin* cliprdr)
{
	STREAM* s;

	s = cliprdr_packet_new(CB_FORMAT_LIST_RESPONSE, CB_RESPONSE_OK, 0);
	cliprdr_packet_send(cliprdr, s);
}

void cliprdr_process_format_list(cliprdrPlugin* cliprdr, STREAM* s, uint32 dataLen)
{
	RDP_CB_FORMAT_LIST_EVENT* cb_event;
	uint32 format;
	int num_formats;
	int supported;
	int i;

	cb_event = (RDP_CB_FORMAT_LIST_EVENT*)freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR,
		RDP_EVENT_TYPE_CB_FORMAT_LIST, NULL, NULL);

	num_formats = dataLen / 36;
	cb_event->formats = (uint32*)xmalloc(sizeof(uint32) * num_formats);
	cb_event->num_formats = 0;
	if (dataLen > 0)
	{
		cb_event->raw_format_data = (uint8*) xmalloc(dataLen);
		memcpy(cb_event->raw_format_data, stream_get_tail(s), dataLen);
		cb_event->raw_format_data_size = dataLen;
	}

	if (num_formats * 36 != dataLen)
		DEBUG_WARN("dataLen %d not devided by 36!", dataLen);

	for (i = 0; i < num_formats; i++)
	{
		stream_read_uint32(s, format);
		supported = 1;
		switch (format)
		{
			case CB_FORMAT_TEXT:
			case CB_FORMAT_DIB:
			case CB_FORMAT_UNICODETEXT:
				break;

			default:
				if (memcmp(stream_get_tail(s), CFSTR_HTML, sizeof(CFSTR_HTML)) == 0)
				{
					format = CB_FORMAT_HTML;
					break;
				}
				if (memcmp(stream_get_tail(s), CFSTR_PNG, sizeof(CFSTR_PNG)) == 0)
				{
					format = CB_FORMAT_PNG;
					break;
				}
				if (memcmp(stream_get_tail(s), CFSTR_JPEG, sizeof(CFSTR_JPEG)) == 0)
				{
					format = CB_FORMAT_JPEG;
					break;
				}
				if (memcmp(stream_get_tail(s), CFSTR_GIF, sizeof(CFSTR_GIF)) == 0)
				{
					format = CB_FORMAT_GIF;
					break;
				}
				supported = 0;
				break;
		}
		stream_seek(s, 32);

		if (supported)
			cb_event->formats[cb_event->num_formats++] = format;
	}

	svc_plugin_send_event((rdpSvcPlugin*)cliprdr, (RDP_EVENT*)cb_event);
	cliprdr_send_format_list_response(cliprdr);
}

void cliprdr_process_format_list_response(cliprdrPlugin* cliprdr, uint16 msgFlags)
{
	RDP_EVENT* event;

	if ((msgFlags & CB_RESPONSE_FAIL) != 0)
	{
		event = freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR, RDP_EVENT_TYPE_CB_SYNC, NULL, NULL);
		svc_plugin_send_event((rdpSvcPlugin*)cliprdr, event);
	}
}

void cliprdr_process_format_data_request(cliprdrPlugin* cliprdr, STREAM* s)
{
	RDP_CB_DATA_REQUEST_EVENT* cb_event;

	cb_event = (RDP_CB_DATA_REQUEST_EVENT*)freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR,
		RDP_EVENT_TYPE_CB_DATA_REQUEST, NULL, NULL);
	stream_read_uint32(s, cb_event->format);
	svc_plugin_send_event((rdpSvcPlugin*)cliprdr, (RDP_EVENT*)cb_event);
}

void cliprdr_process_format_data_response_event(cliprdrPlugin* cliprdr, RDP_CB_DATA_RESPONSE_EVENT* cb_event)
{
	STREAM* s;

	if (cb_event->size > 0)
	{
		s = cliprdr_packet_new(CB_FORMAT_DATA_RESPONSE, CB_RESPONSE_OK, cb_event->size);
		stream_write(s, cb_event->data, cb_event->size);
	}
	else
	{
		s = cliprdr_packet_new(CB_FORMAT_DATA_RESPONSE, CB_RESPONSE_FAIL, 0);
	}
	cliprdr_packet_send(cliprdr, s);
}

void cliprdr_process_format_data_request_event(cliprdrPlugin* cliprdr, RDP_CB_DATA_REQUEST_EVENT* cb_event)
{
	STREAM* s;

	s = cliprdr_packet_new(CB_FORMAT_DATA_REQUEST, 0, 4);
	stream_write_uint32(s, cb_event->format);
	cliprdr_packet_send(cliprdr, s);
}

void cliprdr_process_format_data_response(cliprdrPlugin* cliprdr, STREAM* s, uint32 dataLen)
{
	RDP_CB_DATA_RESPONSE_EVENT* cb_event;

	cb_event = (RDP_CB_DATA_RESPONSE_EVENT*)freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR,
		RDP_EVENT_TYPE_CB_DATA_RESPONSE, NULL, NULL);

	if (dataLen > 0)
	{
		cb_event->size = dataLen;
		cb_event->data = (uint8*)xmalloc(dataLen);
		memcpy(cb_event->data, stream_get_tail(s), dataLen);
	}

	svc_plugin_send_event((rdpSvcPlugin*)cliprdr, (RDP_EVENT*)cb_event);
}
