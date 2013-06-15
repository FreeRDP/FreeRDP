/**
 * WinPR: Windows Portable Runtime
 * Publisher/Subscriber Pattern
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>

#include <winpr/collections.h>

/**
 * Properties
 */

wEvent* PubSub_Events(wPubSub* pubSub, int* count)
{
	if (count)
		*count = pubSub->count;

	return pubSub->events;
}

/**
 * Methods
 */

wEvent* PubSub_FindEvent(wPubSub* pubSub, const char* EventName)
{
	int index;
	wEvent* event = NULL;

	for (index = 0; pubSub->count; index++)
	{
		if (strcmp(pubSub->events[index].EventName, EventName) == 0)
		{
			event = &(pubSub->events[index]);
			break;
		}
	}

	return event;
}

void PubSub_Publish(wPubSub* pubSub, wEvent* events, int count)
{
	while (pubSub->count + count >= pubSub->size)
	{
		pubSub->size *= 2;
		pubSub->events = (wEvent*) realloc(pubSub->events, pubSub->size);
	}

	CopyMemory(&pubSub->events[pubSub->count], events, count * sizeof(wEvent));
	pubSub->count += count;
}

int PubSub_Subscribe(wPubSub* pubSub, const char* EventName, pEventHandler EventHandler)
{
	wEvent* event;
	int status = -1;

	event = PubSub_FindEvent(pubSub, EventName);

	if (event)
	{
		event->EventHandler = EventHandler;
		status = 0;
	}

	return status;
}

int PubSub_Unsubscribe(wPubSub* pubSub, const char* EventName, pEventHandler EventHandler)
{
	wEvent* event;
	int status = -1;

	event = PubSub_FindEvent(pubSub, EventName);

	if (event)
	{
		event->EventHandler = NULL;
		status = 0;
	}

	return status;
}

int PubSub_OnEvent(wPubSub* pubSub, const char* EventName, void* context, wEventArgs* e)
{
	wEvent* event;
	int status = -1;

	event = PubSub_FindEvent(pubSub, EventName);

	if (event)
	{
		if (event->EventHandler)
		{
			event->EventHandler(context, e);
			status = 1;
		}
		else
		{
			status = 0;
		}
	}

	return status;
}

/**
 * Construction, Destruction
 */

wPubSub* PubSub_New(BOOL synchronized)
{
	wPubSub* pubSub = NULL;

	pubSub = (wPubSub*) malloc(sizeof(wPubSub));

	if (pubSub)
	{
		pubSub->synchronized = synchronized;

		if (pubSub->synchronized)
			pubSub->mutex = CreateMutex(NULL, FALSE, NULL);

		pubSub->count = 0;
		pubSub->size = 64;

		pubSub->events = (wEvent*) malloc(sizeof(wEvent) * pubSub->size);
		ZeroMemory(pubSub->events, sizeof(wEvent) * pubSub->size);
	}

	return pubSub;
}

void PubSub_Free(wPubSub* pubSub)
{
	if (pubSub)
	{
		if (pubSub->synchronized)
			CloseHandle(pubSub->mutex);

		if (pubSub->events)
			free(pubSub->events);

		free(pubSub);
	}
}
