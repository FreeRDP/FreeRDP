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
 * Events (C# Programming Guide)
 * http://msdn.microsoft.com/en-us/library/awbftdfh.aspx
 */

/**
 * Properties
 */

wEventType* PubSub_GetEventTypes(wPubSub* pubSub, int* count)
{
	if (count)
		*count = pubSub->count;

	return pubSub->events;
}

/**
 * Methods
 */

void PubSub_Lock(wPubSub* pubSub)
{
	EnterCriticalSection(&pubSub->lock);
}

void PubSub_Unlock(wPubSub* pubSub)
{
	LeaveCriticalSection(&pubSub->lock);
}

wEventType* PubSub_FindEventType(wPubSub* pubSub, const char* EventName)
{
	int index;
	wEventType* event = NULL;

	for (index = 0; index < pubSub->count; index++)
	{
		if (strcmp(pubSub->events[index].EventName, EventName) == 0)
		{
			event = &(pubSub->events[index]);
			break;
		}
	}

	return event;
}

void PubSub_AddEventTypes(wPubSub* pubSub, wEventType* events, int count)
{
	if (pubSub->synchronized)
		PubSub_Lock(pubSub);

	while (pubSub->count + count >= pubSub->size)
	{
		int new_size;
		wEventType *new_event;

		new_size = pubSub->size * 2;
		new_event = (wEventType*) realloc(pubSub->events, new_size * sizeof(wEventType));
		if (!new_event)
			return;
		pubSub->size = new_size;
		pubSub->events = new_event;
	}

	CopyMemory(&pubSub->events[pubSub->count], events, count * sizeof(wEventType));
	pubSub->count += count;

	if (pubSub->synchronized)
		PubSub_Unlock(pubSub);
}

int PubSub_Subscribe(wPubSub* pubSub, const char* EventName, pEventHandler EventHandler)
{
	wEventType* event;
	int status = -1;

	if (pubSub->synchronized)
		PubSub_Lock(pubSub);

	event = PubSub_FindEventType(pubSub, EventName);

	if (event)
	{
		status = 0;

		if (event->EventHandlerCount <= MAX_EVENT_HANDLERS)
		{
			event->EventHandlers[event->EventHandlerCount] = EventHandler;
			event->EventHandlerCount++;
		}
		else
		{
			status = -1;
		}
	}

	if (pubSub->synchronized)
		PubSub_Unlock(pubSub);

	return status;
}

int PubSub_Unsubscribe(wPubSub* pubSub, const char* EventName, pEventHandler EventHandler)
{
	int index;
	wEventType* event;
	int status = -1;

	if (pubSub->synchronized)
		PubSub_Lock(pubSub);

	event = PubSub_FindEventType(pubSub, EventName);

	if (event)
	{
		status = 0;

		for (index = 0; index < event->EventHandlerCount; index++)
		{
			if (event->EventHandlers[index] == EventHandler)
			{
				event->EventHandlers[index] = NULL;
				event->EventHandlerCount--;
				MoveMemory(&event->EventHandlers[index], &event->EventHandlers[index + 1],
				        (MAX_EVENT_HANDLERS - index - 1) * sizeof(pEventHandler));
				status = 1;
			}
		}
	}

	if (pubSub->synchronized)
		PubSub_Unlock(pubSub);

	return status;
}

int PubSub_OnEvent(wPubSub* pubSub, const char* EventName, void* context, wEventArgs* e)
{
	int index;
	wEventType* event;
	int status = -1;

	if (pubSub->synchronized)
		PubSub_Lock(pubSub);

	event = PubSub_FindEventType(pubSub, EventName);

	if (pubSub->synchronized)
		PubSub_Unlock(pubSub);

	if (event)
	{
		status = 0;

		for (index = 0; index < event->EventHandlerCount; index++)
		{
			if (event->EventHandlers[index])
			{
				event->EventHandlers[index](context, e);
				status++;
			}
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

	if (!pubSub)
		return NULL;

	pubSub->synchronized = synchronized;

	if (pubSub->synchronized && !InitializeCriticalSectionAndSpinCount(&pubSub->lock, 4000))
	{
		free(pubSub);
		return NULL;
	}

	pubSub->count = 0;
	pubSub->size = 64;

	pubSub->events = (wEventType*) calloc(pubSub->size, sizeof(wEventType));
	if (!pubSub->events)
	{
		if (pubSub->synchronized)
			DeleteCriticalSection(&pubSub->lock);
		free(pubSub);
		return NULL;
	}

	return pubSub;
}

void PubSub_Free(wPubSub* pubSub)
{
	if (pubSub)
	{
		if (pubSub->synchronized)
			DeleteCriticalSection(&pubSub->lock);

		free(pubSub->events);
		free(pubSub);
	}
}
