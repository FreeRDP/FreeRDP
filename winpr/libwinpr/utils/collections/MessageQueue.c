/**
 * WinPR: Windows Portable Runtime
 * Message Queue
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
#include <winpr/sysinfo.h>

#include <winpr/collections.h>

/**
 * Message Queue inspired from Windows:
 * http://msdn.microsoft.com/en-us/library/ms632590/
 */

/**
 * Properties
 */

/**
 * Gets an event which is set when the queue is non-empty
 */

HANDLE MessageQueue_Event(wMessageQueue* queue)
{
	return queue->event;
}

/**
 * Gets the queue size
 */

int MessageQueue_Size(wMessageQueue* queue)
{
	return queue->size;
}

/**
 * Methods
 */

BOOL MessageQueue_Wait(wMessageQueue* queue)
{
	BOOL status = FALSE;

	if (WaitForSingleObject(queue->event, INFINITE) == WAIT_OBJECT_0)
		status = TRUE;

	return status;
}

void MessageQueue_Dispatch(wMessageQueue* queue, wMessage* message)
{
	EnterCriticalSection(&queue->lock);

	if (queue->size == queue->capacity)
	{
		int old_capacity;
		int new_capacity;

		old_capacity = queue->capacity;
		new_capacity = queue->capacity * 2;

		queue->capacity = new_capacity;
		queue->array = (wMessage*) realloc(queue->array, sizeof(wMessage) * queue->capacity);
		ZeroMemory(&(queue->array[old_capacity]), old_capacity * sizeof(wMessage));

		if (queue->tail < old_capacity)
		{
			CopyMemory(&(queue->array[old_capacity]), queue->array, queue->tail * sizeof(wMessage));
			queue->tail += old_capacity;
		}
	}

	CopyMemory(&(queue->array[queue->tail]), message, sizeof(wMessage));
	queue->tail = (queue->tail + 1) % queue->capacity;
	queue->size++;

	message = &(queue->array[queue->tail]);
	message->time = (UINT64) GetTickCount();

	if (queue->size > 0)
		SetEvent(queue->event);

	LeaveCriticalSection(&queue->lock);
}

void MessageQueue_Post(wMessageQueue* queue, void* context, UINT32 type, void* wParam, void* lParam)
{
	wMessage message;

	message.context = context;
	message.id = type;
	message.wParam = wParam;
	message.lParam = lParam;

	MessageQueue_Dispatch(queue, &message);
}

void MessageQueue_PostQuit(wMessageQueue* queue, int nExitCode)
{
	MessageQueue_Post(queue, NULL, WMQ_QUIT, (void*) (size_t) nExitCode, NULL);
}

int MessageQueue_Get(wMessageQueue* queue, wMessage* message)
{
	int status = -1;

	if (!MessageQueue_Wait(queue))
		return status;

	EnterCriticalSection(&queue->lock);

	if (queue->size > 0)
	{
		CopyMemory(message, &(queue->array[queue->head]), sizeof(wMessage));
		ZeroMemory(&(queue->array[queue->head]), sizeof(wMessage));
		queue->head = (queue->head + 1) % queue->capacity;
		queue->size--;

		if (queue->size < 1)
			ResetEvent(queue->event);

		status = (message->id != WMQ_QUIT) ? 1 : 0;
	}

	LeaveCriticalSection(&queue->lock);

	return status;
}

int MessageQueue_Peek(wMessageQueue* queue, wMessage* message, BOOL remove)
{
	int status = 0;

	EnterCriticalSection(&queue->lock);

	if (queue->size > 0)
	{
		CopyMemory(message, &(queue->array[queue->head]), sizeof(wMessage));
		status = 1;

		if (remove)
		{
			ZeroMemory(&(queue->array[queue->head]), sizeof(wMessage));
			queue->head = (queue->head + 1) % queue->capacity;
			queue->size--;

			if (queue->size < 1)
				ResetEvent(queue->event);
		}
	}

	LeaveCriticalSection(&queue->lock);

	return status;
}

/**
 * Construction, Destruction
 */

wMessageQueue* MessageQueue_New()
{
	wMessageQueue* queue = NULL;

	queue = (wMessageQueue*) malloc(sizeof(wMessageQueue));

	if (queue)
	{
		queue->head = 0;
		queue->tail = 0;
		queue->size = 0;

		queue->capacity = 32;
		queue->array = (wMessage*) malloc(sizeof(wMessage) * queue->capacity);
		ZeroMemory(queue->array, sizeof(wMessage) * queue->capacity);

		InitializeCriticalSectionAndSpinCount(&queue->lock, 4000);
		queue->event = CreateEvent(NULL, TRUE, FALSE, NULL);
	}

	return queue;
}

void MessageQueue_Free(wMessageQueue* queue)
{
	CloseHandle(queue->event);
	DeleteCriticalSection(&queue->lock);

	free(queue->array);
	free(queue);
}
