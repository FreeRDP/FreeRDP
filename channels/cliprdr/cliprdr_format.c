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
#include <freerdp/utils/hexdump.h>
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

	DEBUG_CLIPRDR("Sending Clipboard Format List");

	if (cb_event->raw_format_data)
	{
		s = cliprdr_packet_new(CB_FORMAT_LIST, 0, cb_event->raw_format_data_size);
		stream_write(s, cb_event->raw_format_data, cb_event->raw_format_data_size);
	}
	else
	{
		STREAM* body = stream_new(0);
		
		for (i = 0; i < cb_event->num_formats; i++)
		{
			const char* name;
			int name_length;

			switch (cb_event->formats[i])
			{
				case CB_FORMAT_HTML:
					name = CFSTR_HTML; name_length = sizeof(CFSTR_HTML);
					break;
				case CB_FORMAT_PNG:
					name = CFSTR_PNG; name_length = sizeof(CFSTR_PNG);
					break;
				case CB_FORMAT_JPEG:
					name = CFSTR_JPEG; name_length = sizeof(CFSTR_JPEG);
					break;
				case CB_FORMAT_GIF:
					name = CFSTR_GIF; name_length = sizeof(CFSTR_GIF);
					break;
				default:
					name = "\0\0"; name_length = 2;
			}
			
			if (!cliprdr->use_long_format_names)
				name_length = 32;
			
			stream_extend(body, stream_get_size(body) + 4 + name_length);

			stream_write_uint32(body, cb_event->formats[i]);
			stream_write(body, name, name_length);
		}
				
		s = cliprdr_packet_new(CB_FORMAT_LIST, 0, stream_get_size(body));
		stream_write(s, stream_get_head(body), stream_get_size(body));
		stream_free(body);
	}

	cliprdr_packet_send(cliprdr, s);
}

static void cliprdr_send_format_list_response(cliprdrPlugin* cliprdr)
{
	STREAM* s;
	DEBUG_CLIPRDR("Sending Clipboard Format List Response");
	s = cliprdr_packet_new(CB_FORMAT_LIST_RESPONSE, CB_RESPONSE_OK, 0);
	cliprdr_packet_send(cliprdr, s);
}

void cliprdr_process_short_format_names(cliprdrPlugin* cliprdr, STREAM* s, uint32 length, uint16 flags)
{
	int i;
	boolean ascii;
	int num_formats;
	CLIPRDR_FORMAT_NAME* format_name;

	num_formats = length / 36;

	if (num_formats <= 0)
	{
		cliprdr->format_names = NULL;
		cliprdr->num_format_names = 0;
		return;
	}

	if (num_formats * 36 != length)
		DEBUG_WARN("dataLen %d not divided by 36!", length);

	ascii = (flags & CB_ASCII_NAMES) ? true : false;

	cliprdr->format_names = (CLIPRDR_FORMAT_NAME*) xmalloc(sizeof(CLIPRDR_FORMAT_NAME) * num_formats);
	cliprdr->num_format_names = num_formats;

	for (i = 0; i < num_formats; i++)
	{
		format_name = &cliprdr->format_names[i];

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
	}
}

void cliprdr_process_long_format_names(cliprdrPlugin* cliprdr, STREAM* s, uint32 length, uint16 flags)
{
	int allocated_formats = 8;
	uint8* end_mark;
	CLIPRDR_FORMAT_NAME* format_name;
	
	stream_get_mark(s, end_mark);
	end_mark += length;
		
	cliprdr->format_names = (CLIPRDR_FORMAT_NAME*) xmalloc(sizeof(CLIPRDR_FORMAT_NAME) * allocated_formats);
	cliprdr->num_format_names = 0;

	while (stream_get_left(s) >= 6)
	{
		uint8* p;
		int name_len;
		
		if (cliprdr->num_format_names >= allocated_formats)
		{
			allocated_formats *= 2;
			cliprdr->format_names = (CLIPRDR_FORMAT_NAME*) xrealloc(cliprdr->format_names,
					sizeof(CLIPRDR_FORMAT_NAME) * allocated_formats);
		}
		
		format_name = &cliprdr->format_names[cliprdr->num_format_names++];
		stream_read_uint32(s, format_name->id);
		
		format_name->name = NULL;
		format_name->length = 0;

		for (p = stream_get_tail(s), name_len = 0; p + 1 < end_mark; p += 2, name_len += 2)
		{
			if (*((unsigned short*) p) == 0)
				break;
		}
		
		format_name->name = freerdp_uniconv_in(cliprdr->uniconv, stream_get_tail(s), name_len);
		format_name->length = strlen(format_name->name);
		stream_seek(s, name_len + 2);
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

	if (cliprdr->num_format_names > 0)
		cb_event->formats = (uint32*) xmalloc(sizeof(uint32) * cliprdr->num_format_names);

	cb_event->num_formats = 0;

	for (i = 0; i < cliprdr->num_format_names; i++)
	{
		supported = true;
		format_name = &cliprdr->format_names[i];
		format = format_name->id;

		switch (format)
		{
			case CB_FORMAT_TEXT:
			case CB_FORMAT_DIB:
			case CB_FORMAT_UNICODETEXT:
				break;

			default:

				if (format_name->length > 0)
				{
					DEBUG_CLIPRDR("format: %s", format_name->name);

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
				}
				else
				{
					supported = false;
				}
				
				break;
		}

		if (supported)
			cb_event->formats[cb_event->num_formats++] = format;

		if (format_name->length > 0)
			xfree(format_name->name);
	}

	xfree(cliprdr->format_names);
	cliprdr->format_names = NULL;

	cliprdr->num_format_names = 0;

	svc_plugin_send_event((rdpSvcPlugin*) cliprdr, (RDP_EVENT*) cb_event);
	cliprdr_send_format_list_response(cliprdr);
}

void cliprdr_process_format_list_response(cliprdrPlugin* cliprdr, STREAM* s, uint32 dataLen, uint16 msgFlags)
{
	/* where is this documented? */
#if 0
	RDP_EVENT* event;

	if ((msgFlags & CB_RESPONSE_FAIL) != 0)
	{
		event = freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR, RDP_EVENT_TYPE_CB_MONITOR_READY, NULL, NULL);
		svc_plugin_send_event((rdpSvcPlugin*) cliprdr, event);
	}
#endif
}

void cliprdr_process_format_data_request(cliprdrPlugin* cliprdr, STREAM* s, uint32 dataLen, uint16 msgFlags)
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

	DEBUG_CLIPRDR("Sending Format Data Response");

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

	DEBUG_CLIPRDR("Sending Format Data Request");

	s = cliprdr_packet_new(CB_FORMAT_DATA_REQUEST, 0, 4);
	stream_write_uint32(s, cb_event->format);
	cliprdr_packet_send(cliprdr, s);
}

void cliprdr_process_format_data_response(cliprdrPlugin* cliprdr, STREAM* s, uint32 dataLen, uint16 msgFlags)
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
