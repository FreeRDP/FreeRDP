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
#include <freerdp/utils/event.h>

FRDP_EVENT* freerdp_event_new(uint32 event_type, FRDP_EVENT_CALLBACK callback, void* user_data)
{
	FRDP_EVENT* event = NULL;

	switch (event_type)
	{
		case FRDP_EVENT_TYPE_DEBUG:
			event = (FRDP_EVENT*)xmalloc(sizeof(FRDP_EVENT));
			memset(event, 0, sizeof(FRDP_EVENT));
			break;
		case FRDP_EVENT_TYPE_VIDEO_FRAME:
			event = (FRDP_EVENT*)xmalloc(sizeof(FRDP_VIDEO_FRAME_EVENT));
			memset(event, 0, sizeof(FRDP_VIDEO_FRAME_EVENT));
			break;
		case FRDP_EVENT_TYPE_REDRAW:
			event = (FRDP_EVENT*)xmalloc(sizeof(FRDP_REDRAW_EVENT));
			memset(event, 0, sizeof(FRDP_REDRAW_EVENT));
			break;
	}
	if (event != NULL)
	{
		event->event_type = event_type;
		event->event_callback = callback;
		event->user_data = user_data;
	}

	return event;
}

void freerdp_event_free(FRDP_EVENT* event)
{
	if (event != NULL)
	{
		if (event->event_callback != NULL)
			event->event_callback(event);

		switch (event->event_type)
		{
			case FRDP_EVENT_TYPE_VIDEO_FRAME:
				{
					FRDP_VIDEO_FRAME_EVENT* vevent = (FRDP_VIDEO_FRAME_EVENT*)event;

					xfree(vevent->frame_data);
					xfree(vevent->visible_rects);
				}
				break;
		}
		xfree(event);
	}
}
