/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/print.h>

#include <freerdp/types.h>
#include <freerdp/constants.h>
#include <freerdp/utils/svc_plugin.h>
#include <freerdp/client/cliprdr.h>

#include "cliprdr_main.h"
#include "cliprdr_format.h"

#define CFSTR_HTML      "H\0T\0M\0L\0 \0F\0o\0r\0m\0a\0t\0\0"
#define CFSTR_PNG       "P\0N\0G\0\0"
#define CFSTR_JPEG      "J\0F\0I\0F\0\0"
#define CFSTR_GIF       "G\0I\0F\0\0"

void cliprdr_process_format_list_event(cliprdrPlugin* cliprdr, RDP_CB_FORMAT_LIST_EVENT* cb_event)
{
	int i;
	wStream* s;

	DEBUG_CLIPRDR("Sending Clipboard Format List");

	if (cb_event->raw_format_data)
	{
		s = cliprdr_packet_new(CB_FORMAT_LIST, 0, cb_event->raw_format_data_size);
		Stream_Write(s, cb_event->raw_format_data, cb_event->raw_format_data_size);
	}
	else
	{
		wStream* body = Stream_New(NULL, 64);
		
		for (i = 0; i < cb_event->num_formats; i++)
		{
			const char* name;
			int name_length, short_name_length = 32, x;

			switch (cb_event->formats[i])
			{
				case CB_FORMAT_HTML:
					name = CFSTR_HTML;
					name_length = sizeof(CFSTR_HTML);
					break;
				case CB_FORMAT_PNG:
					name = CFSTR_PNG;
					name_length = sizeof(CFSTR_PNG);
					break;
				case CB_FORMAT_JPEG:
					name = CFSTR_JPEG;
					name_length = sizeof(CFSTR_JPEG);
					break;
				case CB_FORMAT_GIF:
					name = CFSTR_GIF;
					name_length = sizeof(CFSTR_GIF);
					break;
				default:
					name = "\0\0";
					name_length = 2;
					break;
			}
			
			if (!cliprdr->use_long_format_names)
			{				
				x = (name_length > short_name_length) ?
					name_length : short_name_length;

				Stream_EnsureRemainingCapacity(body, 4 + short_name_length);
				Stream_Write_UINT32(body, cb_event->formats[i]);
				Stream_Write(body, name, x);

				while (x++ < short_name_length)
					Stream_Write(body, "\0", 1);
			}
			else
			{
				Stream_EnsureRemainingCapacity(body, 4 + name_length);
				Stream_Write_UINT32(body, cb_event->formats[i]);
				Stream_Write(body, name, name_length);
			}
		}
				
		Stream_SealLength(body);
		s = cliprdr_packet_new(CB_FORMAT_LIST, 0, Stream_Length(body));
		Stream_Write(s, Stream_Buffer(body), Stream_Length(body));
		Stream_Free(body, TRUE);
	}

	cliprdr_packet_send(cliprdr, s);
}

static void cliprdr_send_format_list_response(cliprdrPlugin* cliprdr)
{
	wStream* s;
	DEBUG_CLIPRDR("Sending Clipboard Format List Response");
	s = cliprdr_packet_new(CB_FORMAT_LIST_RESPONSE, CB_RESPONSE_OK, 0);
	cliprdr_packet_send(cliprdr, s);
}

void cliprdr_process_short_format_names(cliprdrPlugin* cliprdr, wStream* s, UINT32 length, UINT16 flags)
{
	int i;
	BOOL ascii;
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

	ascii = (flags & CB_ASCII_NAMES) ? TRUE : FALSE;

	cliprdr->format_names = (CLIPRDR_FORMAT_NAME*) malloc(sizeof(CLIPRDR_FORMAT_NAME) * num_formats);
	cliprdr->num_format_names = num_formats;

	for (i = 0; i < num_formats; i++)
	{
		format_name = &cliprdr->format_names[i];

		Stream_Read_UINT32(s, format_name->id);

		if (ascii)
		{
			format_name->name = _strdup((char*) s->pointer);
			format_name->length = strlen(format_name->name);
		}
		else
		{
			format_name->name = NULL;
			format_name->length = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) s->pointer, 32 / 2, &format_name->name, 0, NULL, NULL);
		}

		Stream_Seek(s, 32);
	}
}

void cliprdr_process_long_format_names(cliprdrPlugin* cliprdr, wStream* s, UINT32 length, UINT16 flags)
{
	int allocated_formats = 8;
	BYTE* end_mark;
	CLIPRDR_FORMAT_NAME* format_name;
	
	Stream_GetPointer(s, end_mark);
	end_mark += length;
		
	cliprdr->format_names = (CLIPRDR_FORMAT_NAME*) malloc(sizeof(CLIPRDR_FORMAT_NAME) * allocated_formats);
	cliprdr->num_format_names = 0;

	while (Stream_GetRemainingLength(s) >= 6)
	{
		BYTE* p;
		int name_len;
		
		if (cliprdr->num_format_names >= allocated_formats)
		{
			allocated_formats *= 2;
			cliprdr->format_names = (CLIPRDR_FORMAT_NAME*) realloc(cliprdr->format_names,
					sizeof(CLIPRDR_FORMAT_NAME) * allocated_formats);
		}
		
		format_name = &cliprdr->format_names[cliprdr->num_format_names++];
		Stream_Read_UINT32(s, format_name->id);
		
		format_name->name = NULL;
		format_name->length = 0;

		for (p = Stream_Pointer(s), name_len = 0; p + 1 < end_mark; p += 2, name_len += 2)
		{
			if (*((unsigned short*) p) == 0)
				break;
		}
		
		format_name->length = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) Stream_Pointer(s), name_len / 2, &format_name->name, 0, NULL, NULL);

		Stream_Seek(s, name_len + 2);
	}
}

void cliprdr_process_format_list(cliprdrPlugin* cliprdr, wStream* s, UINT32 dataLen, UINT16 msgFlags)
{
	int i;
	UINT32 format;
	BOOL supported;
	CLIPRDR_FORMAT_NAME* format_name;
	RDP_CB_FORMAT_LIST_EVENT* cb_event;

	cb_event = (RDP_CB_FORMAT_LIST_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_FormatList, NULL, NULL);

	if (dataLen > 0)
	{
		cb_event->raw_format_data = (BYTE*) malloc(dataLen);
		memcpy(cb_event->raw_format_data, Stream_Pointer(s), dataLen);
		cb_event->raw_format_data_size = dataLen;
	}

	if (cliprdr->use_long_format_names)
		cliprdr_process_long_format_names(cliprdr, s, dataLen, msgFlags);
	else
		cliprdr_process_short_format_names(cliprdr, s, dataLen, msgFlags);

	if (cliprdr->num_format_names > 0)
		cb_event->formats = (UINT32*) malloc(sizeof(UINT32) * cliprdr->num_format_names);

	cb_event->num_formats = 0;

	for (i = 0; i < cliprdr->num_format_names; i++)
	{
		supported = TRUE;
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
					supported = FALSE;
				}
				
				break;
		}

		if (supported)
			cb_event->formats[cb_event->num_formats++] = format;

		if (format_name->length > 0)
			free(format_name->name);
	}

	free(cliprdr->format_names);
	cliprdr->format_names = NULL;

	cliprdr->num_format_names = 0;

	svc_plugin_send_event((rdpSvcPlugin*) cliprdr, (wMessage*) cb_event);
	cliprdr_send_format_list_response(cliprdr);
}

void cliprdr_process_format_list_response(cliprdrPlugin* cliprdr, wStream* s, UINT32 dataLen, UINT16 msgFlags)
{
	/* http://msdn.microsoft.com/en-us/library/hh872154.aspx */
	wMessage* event;

	if ((msgFlags & CB_RESPONSE_FAIL) != 0)
	{
		/* In case of an error the clipboard will not be synchronized with the server.
		 * Post this event to restart format negociation and data transfer. */
		event = freerdp_event_new(CliprdrChannel_Class, CliprdrChannel_MonitorReady, NULL, NULL);
		svc_plugin_send_event((rdpSvcPlugin*) cliprdr, event);
	}
}

void cliprdr_process_format_data_request(cliprdrPlugin* cliprdr, wStream* s, UINT32 dataLen, UINT16 msgFlags)
{
	RDP_CB_DATA_REQUEST_EVENT* cb_event;

	cb_event = (RDP_CB_DATA_REQUEST_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_DataRequest, NULL, NULL);

	Stream_Read_UINT32(s, cb_event->format);
	svc_plugin_send_event((rdpSvcPlugin*) cliprdr, (wMessage*) cb_event);
}

void cliprdr_process_format_data_response_event(cliprdrPlugin* cliprdr, RDP_CB_DATA_RESPONSE_EVENT* cb_event)
{
	wStream* s;

	DEBUG_CLIPRDR("Sending Format Data Response");

	if (cb_event->size > 0)
	{
		s = cliprdr_packet_new(CB_FORMAT_DATA_RESPONSE, CB_RESPONSE_OK, cb_event->size);
		Stream_Write(s, cb_event->data, cb_event->size);
	}
	else
	{
		s = cliprdr_packet_new(CB_FORMAT_DATA_RESPONSE, CB_RESPONSE_FAIL, 0);
	}

	cliprdr_packet_send(cliprdr, s);
}

void cliprdr_process_format_data_request_event(cliprdrPlugin* cliprdr, RDP_CB_DATA_REQUEST_EVENT* cb_event)
{
	wStream* s;

	DEBUG_CLIPRDR("Sending Format Data Request");

	s = cliprdr_packet_new(CB_FORMAT_DATA_REQUEST, 0, 4);
	Stream_Write_UINT32(s, cb_event->format);
	cliprdr_packet_send(cliprdr, s);
}

void cliprdr_process_format_data_response(cliprdrPlugin* cliprdr, wStream* s, UINT32 dataLen, UINT16 msgFlags)
{
	RDP_CB_DATA_RESPONSE_EVENT* cb_event;

	cb_event = (RDP_CB_DATA_RESPONSE_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_DataResponse, NULL, NULL);

	if (dataLen > 0)
	{
		cb_event->size = dataLen;
		cb_event->data = (BYTE*) malloc(dataLen);
		CopyMemory(cb_event->data, Stream_Pointer(s), dataLen);
	}

	svc_plugin_send_event((rdpSvcPlugin*) cliprdr, (wMessage*) cb_event);
}
