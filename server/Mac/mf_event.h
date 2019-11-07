/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * OS X Server Event Handling
 *
 * Copyright 2012 Corey Clayton <can.of.tuna@gmail.com>
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

#ifndef FREERDP_SERVER_MAC_EVENT_H
#define FREERDP_SERVER_MAC_EVENT_H

typedef struct mf_event mfEvent;
typedef struct mf_event_queue mfEventQueue;
typedef struct mf_event_region mfEventRegion;

#include <pthread.h>
#include "mfreerdp.h"

//#include "mf_peer.h"

enum mf_event_type
{
	FREERDP_SERVER_MAC_EVENT_TYPE_REGION,
	FREERDP_SERVER_MAC_EVENT_TYPE_FRAME_TICK
};

struct mf_event
{
	int type;
};

struct mf_event_queue
{
	int size;
	int count;
	int pipe_fd[2];
	mfEvent** events;
	pthread_mutex_t mutex;
};

struct mf_event_region
{
	int type;

	int x;
	int y;
	int width;
	int height;
};

void mf_event_push(mfEventQueue* event_queue, mfEvent* event);
mfEvent* mf_event_peek(mfEventQueue* event_queue);
mfEvent* mf_event_pop(mfEventQueue* event_queue);

mfEventRegion* mf_event_region_new(int x, int y, int width, int height);
void mf_event_region_free(mfEventRegion* event_region);

mfEvent* mf_event_new(int type);
void mf_event_free(mfEvent* event);

mfEventQueue* mf_event_queue_new(void);
void mf_event_queue_free(mfEventQueue* event_queue);

#endif /* FREERDP_SERVER_MAC_EVENT_H */
