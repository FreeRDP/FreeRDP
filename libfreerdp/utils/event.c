/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Events
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#include <freerdp/message.h>
#include <freerdp/utils/event.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/tsmf.h>
#include <freerdp/rail.h>

static RDP_EVENT* freerdp_cliprdr_event_new(UINT16 event_type)
{
	RDP_EVENT* event = NULL;

	switch (event_type)
	{
		case CliprdrChannel_MonitorReady:
			event = (RDP_EVENT*) malloc(sizeof(RDP_CB_MONITOR_READY_EVENT));
			ZeroMemory(event, sizeof(RDP_CB_MONITOR_READY_EVENT));
			event->id = MakeMessageId(CliprdrChannel, MonitorReady);
			break;

		case CliprdrChannel_FormatList:
			event = (RDP_EVENT*) malloc(sizeof(RDP_CB_FORMAT_LIST_EVENT));
			ZeroMemory(event, sizeof(RDP_CB_FORMAT_LIST_EVENT));
			event->id = MakeMessageId(CliprdrChannel, FormatList);
			break;

		case CliprdrChannel_DataRequest:
			event = (RDP_EVENT*) malloc(sizeof(RDP_CB_DATA_REQUEST_EVENT));
			ZeroMemory(event, sizeof(RDP_CB_DATA_REQUEST_EVENT));
			event->id = MakeMessageId(CliprdrChannel, DataRequest);
			break;

		case CliprdrChannel_DataResponse:
			event = (RDP_EVENT*) malloc(sizeof(RDP_CB_DATA_RESPONSE_EVENT));
			ZeroMemory(event, sizeof(RDP_CB_DATA_RESPONSE_EVENT));
			event->id = MakeMessageId(CliprdrChannel, DataResponse);
			break;
	}

	return event;
}

static RDP_EVENT* freerdp_tsmf_event_new(UINT16 event_type)
{
	RDP_EVENT* event = NULL;

	switch (event_type)
	{
		case TsmfChannel_VideoFrame:
			event = (RDP_EVENT*) malloc(sizeof(RDP_VIDEO_FRAME_EVENT));
			ZeroMemory(event, sizeof(RDP_VIDEO_FRAME_EVENT));
			break;

		case TsmfChannel_Redraw:
			event = (RDP_EVENT*) malloc(sizeof(RDP_REDRAW_EVENT));
			ZeroMemory(event, sizeof(RDP_REDRAW_EVENT));
			break;
	}

	return event;
}

static RDP_EVENT* freerdp_rail_event_new(UINT16 event_type)
{
	RDP_EVENT* event = NULL;

	event = (RDP_EVENT*) malloc(sizeof(RDP_EVENT));
	ZeroMemory(event, sizeof(RDP_EVENT));

	return event;
}

RDP_EVENT* freerdp_event_new(UINT16 event_class, UINT16 event_type,
	RDP_EVENT_CALLBACK on_event_free_callback, void* user_data)
{
	RDP_EVENT* event = NULL;

	switch (event_class)
	{
		case DebugChannel_Class:
			event = (RDP_EVENT*) malloc(sizeof(RDP_EVENT));
			ZeroMemory(event, sizeof(RDP_EVENT));
			break;

		case CliprdrChannel_Class:
			event = freerdp_cliprdr_event_new(event_type);
			break;

		case TsmfChannel_Class:
			event = freerdp_tsmf_event_new(event_type);
			break;

		case RailChannel_Class:
			event = freerdp_rail_event_new(event_type);
			break;
	}

	if (event)
	{
		event->on_event_free_callback = on_event_free_callback;
		event->user_data = user_data;

		event->id = GetMessageId(event_class, event_type);
	}

	return event;
}

static void freerdp_cliprdr_event_free(RDP_EVENT* event)
{
	switch (GetMessageType(event->id))
	{
		case CliprdrChannel_FormatList:
			{
				RDP_CB_FORMAT_LIST_EVENT* cb_event = (RDP_CB_FORMAT_LIST_EVENT*) event;
				free(cb_event->formats);
				free(cb_event->raw_format_data);
			}
			break;

		case CliprdrChannel_DataResponse:
			{
				RDP_CB_DATA_RESPONSE_EVENT* cb_event = (RDP_CB_DATA_RESPONSE_EVENT*) event;
				free(cb_event->data);
			}
			break;
	}
}

static void freerdp_tsmf_event_free(RDP_EVENT* event)
{
	switch (GetMessageType(event->id))
	{
		case TsmfChannel_VideoFrame:
			{
				RDP_VIDEO_FRAME_EVENT* vevent = (RDP_VIDEO_FRAME_EVENT*)event;
				free(vevent->frame_data);
				free(vevent->visible_rects);
			}
			break;
	}
}

static void freerdp_rail_event_free(RDP_EVENT* event)
{

}

void freerdp_event_free(RDP_EVENT* event)
{
	if (event)
	{
		if (event->on_event_free_callback)
			event->on_event_free_callback(event);

		switch (GetMessageClass(event->id))
		{
			case CliprdrChannel_Class:
				freerdp_cliprdr_event_free(event);
				break;

			case TsmfChannel_Class:
				freerdp_tsmf_event_free(event);
				break;

			case RailChannel_Class:
				freerdp_rail_event_free(event);
				break;
		}

		free(event);
	}
}
