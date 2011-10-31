/**
 * FreeRDP: A Remote Desktop Protocol client.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/event.h>
#include <freerdp/plugins/cliprdr.h>
#include <freerdp/plugins/tsmf.h>
#include <freerdp/rail.h>

static RDP_EVENT* freerdp_cliprdr_event_new(uint16 event_type)
{
	RDP_EVENT* event = NULL;

	switch (event_type)
	{
		case RDP_EVENT_TYPE_CB_MONITOR_READY:
			event = (RDP_EVENT*) xnew(RDP_CB_MONITOR_READY_EVENT);
			break;
		case RDP_EVENT_TYPE_CB_FORMAT_LIST:
			event = (RDP_EVENT*) xnew(RDP_CB_FORMAT_LIST_EVENT);
			break;
		case RDP_EVENT_TYPE_CB_DATA_REQUEST:
			event = (RDP_EVENT*) xnew(RDP_CB_DATA_REQUEST_EVENT);
			break;
		case RDP_EVENT_TYPE_CB_DATA_RESPONSE:
			event = (RDP_EVENT*) xnew(RDP_CB_DATA_RESPONSE_EVENT);
			break;
	}

	return event;
}

static RDP_EVENT* freerdp_tsmf_event_new(uint16 event_type)
{
	RDP_EVENT* event = NULL;

	switch (event_type)
	{
		case RDP_EVENT_TYPE_TSMF_VIDEO_FRAME:
			event = (RDP_EVENT*) xnew(RDP_VIDEO_FRAME_EVENT);
			break;
		case RDP_EVENT_TYPE_TSMF_REDRAW:
			event = (RDP_EVENT*) xnew(RDP_REDRAW_EVENT);
			break;
	}

	return event;
}

static RDP_EVENT* freerdp_rail_event_new(uint16 event_type)
{
	RDP_EVENT* event = NULL;
	event = xnew(RDP_EVENT);
	return event;
}

RDP_EVENT* freerdp_event_new(uint16 event_class, uint16 event_type,
	RDP_EVENT_CALLBACK on_event_free_callback, void* user_data)
{
	RDP_EVENT* event = NULL;

	switch (event_class)
	{
		case RDP_EVENT_CLASS_DEBUG:
			event = xnew(RDP_EVENT);
			break;
		case RDP_EVENT_CLASS_CLIPRDR:
			event = freerdp_cliprdr_event_new(event_type);
			break;
		case RDP_EVENT_CLASS_TSMF:
			event = freerdp_tsmf_event_new(event_type);
			break;
		case RDP_EVENT_CLASS_RAIL:
			event = freerdp_rail_event_new(event_type);
			break;
	}

	if (event != NULL)
	{
		event->event_class = event_class;
		event->event_type = event_type;
		event->on_event_free_callback = on_event_free_callback;
		event->user_data = user_data;
	}

	return event;
}

static void freerdp_cliprdr_event_free(RDP_EVENT* event)
{
	switch (event->event_type)
	{
		case RDP_EVENT_TYPE_CB_FORMAT_LIST:
			{
				RDP_CB_FORMAT_LIST_EVENT* cb_event = (RDP_CB_FORMAT_LIST_EVENT*)event;
				xfree(cb_event->formats);
				xfree(cb_event->raw_format_data);
			}
			break;
		case RDP_EVENT_TYPE_CB_DATA_RESPONSE:
			{
				RDP_CB_DATA_RESPONSE_EVENT* cb_event = (RDP_CB_DATA_RESPONSE_EVENT*)event;
				xfree(cb_event->data);
			}
			break;
	}
}

static void freerdp_tsmf_event_free(RDP_EVENT* event)
{
	switch (event->event_type)
	{
		case RDP_EVENT_TYPE_TSMF_VIDEO_FRAME:
			{
				RDP_VIDEO_FRAME_EVENT* vevent = (RDP_VIDEO_FRAME_EVENT*)event;
				xfree(vevent->frame_data);
				xfree(vevent->visible_rects);
			}
			break;
	}
}

static void freerdp_rail_event_free(RDP_EVENT* event)
{
}

void freerdp_event_free(RDP_EVENT* event)
{
	if (event != NULL)
	{
		if (event->on_event_free_callback != NULL)
			event->on_event_free_callback(event);

		switch (event->event_class)
		{
			case RDP_EVENT_CLASS_CLIPRDR:
				freerdp_cliprdr_event_free(event);
				break;
			case RDP_EVENT_CLASS_TSMF:
				freerdp_tsmf_event_free(event);
				break;
			case RDP_EVENT_CLASS_RAIL:
				freerdp_rail_event_free(event);
				break;
		}
		xfree(event);
	}
}
