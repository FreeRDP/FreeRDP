/**
 * FreeRDP: A Remote Desktop Protocol Client
 * X11 Server Event Handling
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __XF_EVENT_H
#define __XF_EVENT_H

typedef struct xf_event xfEvent;
typedef struct xf_event_queue xfEventQueue;
typedef struct xf_event_region xfEventRegion;

#include <pthread.h>
#include "xfreerdp.h"

#include "xf_peer.h"

enum xf_event_type
{
	XF_EVENT_TYPE_REGION,
	XF_EVENT_TYPE_FRAME_TICK
};

struct xf_event
{
	int type;
};

struct xf_event_queue
{
	int size;
	int count;
	int pipe_fd[2];
	xfEvent** events;
	pthread_mutex_t mutex;
};

struct xf_event_region
{
	int type;

	int x;
	int y;
	int width;
	int height;
};

void xf_event_push(xfEventQueue* event_queue, xfEvent* event);
xfEvent* xf_event_peek(xfEventQueue* event_queue);
xfEvent* xf_event_pop(xfEventQueue* event_queue);

xfEventRegion* xf_event_region_new(int x, int y, int width, int height);
void xf_event_region_free(xfEventRegion* event_region);

xfEvent* xf_event_new(int type);
void xf_event_free(xfEvent* event);

xfEventQueue* xf_event_queue_new();
void xf_event_queue_free(xfEventQueue* event_queue);

#endif /* __XF_EVENT_H */
