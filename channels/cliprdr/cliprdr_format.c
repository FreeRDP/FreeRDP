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
#include <freerdp/types.h>
#include <freerdp/constants.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/unicode.h>
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
	int i;
	STREAM* s;

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

void cliprdr_process_short_format_names(cliprdrPlugin* cliprdr, STREAM* s, uint32 length, uint16 flags)
{
	boolean ascii;
	int num_formats;
	uint8* end_mark;
	CLIPRDR_FORMAT_NAME* format_name;

	num_formats = length / 36;

	if (num_formats * 36 != length)
		DEBUG_WARN("dataLen %d not divided by 36!", length);

	ascii = (flags & CB_ASCII_NAMES) ? True : False;

	stream_get_mark(s, end_mark);
	end_mark += length;

	cliprdr->format_names = (CLIPRDR_FORMAT_NAME*) xmalloc(sizeof(CLIPRDR_FORMAT_NAME) * num_formats);
	cliprdr->num_format_names = num_formats;
	format_name = cliprdr->format_names;

	while (s->p < end_mark)
	{
		stream_read_uint32(s, format_name->id);

		if (ascii)
		{
			format_name->name = xstrdup((char*) s->p);
			format_name->length = strlen(format_name->name);
		}
		else
		{
			format_name->name = freerdp_uniconv_in(cliprdr->uniconv, s->p, 32);
			format_name->length = strlen(format_name->name);
		}

		stream_seek(s, 32);

		format_name++;
	}
}

void cliprdr_process_long_format_names(cliprdrPlugin* cliprdr, STREAM* s, uint32 length, uint16 flags)
{
	int num_formats;
	uint8* start_mark;
	uint8* end_mark;
	uint16 terminator;
	CLIPRDR_FORMAT_NAME* format_name;

	num_formats = 0;
	stream_get_mark(s, start_mark);
	stream_get_mark(s, end_mark);
	end_mark += length;

	while (s->p < end_mark)
	{
		stream_seek_uint32(s);

		do
		{
			stream_read_uint16(s, terminator);
		}
		while (terminator != 0x0000);

		num_formats++;
	}

	stream_set_mark(s, start_mark);

	cliprdr->format_names = (CLIPRDR_FORMAT_NAME*) xmalloc(sizeof(CLIPRDR_FORMAT_NAME) * num_formats);
	cliprdr->num_format_names = num_formats;
	format_name = cliprdr->format_names;

	while (s->p < end_mark)
	{
		stream_read_uint32(s, format_name->id);

		format_name->name = freerdp_uniconv_in(cliprdr->uniconv, s->p, 32);
		format_name->length = strlen(format_name->name);
		stream_seek(s, format_name->length);

		format_name++;
	}
}

void cliprdr_process_format_list(cliprdrPlugin* cliprdr, STREAM* s, uint32 dataLen, uint16 msgFlags)
{
	int i;
	uint32 format;
	boolean supported;
	CLIPRDR_FORMAT_NAME* format_name;
	RDP_CB_FORMAT_LIST_EVENT* cb_event;

	cb_event = (RDP_CB_FORMAT_LIST_EVENT*) freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR,
		RDP_EVENT_TYPE_CB_FORMAT_LIST, NULL, NULL);

	if (dataLen > 0)
	{
		cb_event->raw_format_data = (uint8*) xmalloc(dataLen);
		memcpy(cb_event->raw_format_data, stream_get_tail(s), dataLen);
		cb_event->raw_format_data_size = dataLen;
	}

	if (cliprdr->use_long_format_names)
		cliprdr_process_long_format_names(cliprdr, s, dataLen, msgFlags);
	else
		cliprdr_process_short_format_names(cliprdr, s, dataLen, msgFlags);

	format_name = cliprdr->format_names;
	cb_event->num_formats = cliprdr->num_format_names;
	cb_event->formats = (uint32*) xmalloc(sizeof(uint32) * cb_event->num_formats);

	for (i = 0; i < cliprdr->num_format_names; i++)
	{
		supported = True;
		format = format_name->id;

		switch (format)
		{
			case CB_FORMAT_TEXT:
			case CB_FORMAT_DIB:
			case CB_FORMAT_UNICODETEXT:
				break;

			default:
				if (strcmp(format_name->name, "HTML Format") == 0)
				{
					format = CB_FORMAT_HTML;
					break;
				}
				if (strcmp(format_name->name, "PNG") == 0)
				{
					format = CB_FORMAT_PNG;
					break;
				}
				if (strcmp(format_name->name, "JFIF") == 0)
				{
					format = CB_FORMAT_JPEG;
					break;
				}
				if (strcmp(format_name->name, "GIF") == 0)
				{
					format = CB_FORMAT_GIF;
					break;
				}

				supported = False;
				break;
		}

		if (supported)
			cb_event->formats[cb_event->num_formats++] = format;

		format_name++;
	}

	svc_plugin_send_event((rdpSvcPlugin*) cliprdr, (RDP_EVENT*) cb_event);
	cliprdr_send_format_list_response(cliprdr);
}

void cliprdr_process_format_list_response(cliprdrPlugin* cliprdr, uint16 msgFlags)
{
	RDP_EVENT* event;

	if ((msgFlags & CB_RESPONSE_FAIL) != 0)
	{
		event = freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR, RDP_EVENT_TYPE_CB_MONITOR_READY, NULL, NULL);
		svc_plugin_send_event((rdpSvcPlugin*) cliprdr, event);
	}
}

void cliprdr_process_format_data_request(cliprdrPlugin* cliprdr, STREAM* s)
{
	RDP_CB_DATA_REQUEST_EVENT* cb_event;

	cb_event = (RDP_CB_DATA_REQUEST_EVENT*) freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR,
		RDP_EVENT_TYPE_CB_DATA_REQUEST, NULL, NULL);

	stream_read_uint32(s, cb_event->format);
	svc_plugin_send_event((rdpSvcPlugin*) cliprdr, (RDP_EVENT*) cb_event);
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

	cb_event = (RDP_CB_DATA_RESPONSE_EVENT*) freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR,
		RDP_EVENT_TYPE_CB_DATA_RESPONSE, NULL, NULL);

	if (dataLen > 0)
	{
		cb_event->size = dataLen;
		cb_event->data = (uint8*) xmalloc(dataLen);
		memcpy(cb_event->data, stream_get_tail(s), dataLen);
	}

	svc_plugin_send_event((rdpSvcPlugin*) cliprdr, (RDP_EVENT*) cb_event);
}
