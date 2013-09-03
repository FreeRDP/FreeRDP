/**
 * WinPR: Windows Portable Runtime
 * System.Collections.Queue
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
 * C equivalent of the C# Queue Class:
 * http://msdn.microsoft.com/en-us/library/system.collections.queue.aspx
 */

/**
 * Properties
 */

/**
 * Gets the number of elements contained in the Queue.
 */

int Queue_Count(wQueue* queue)
{
	return queue->size;
}

/**
 * Lock access to the ArrayList
 */

void Queue_Lock(wQueue* queue)
{
	EnterCriticalSection(&queue->lock);
}

/**
 * Unlock access to the ArrayList
 */

void Queue_Unlock(wQueue* queue)
{
	LeaveCriticalSection(&queue->lock);
}

/**
 * Gets an event which is set when the queue is non-empty
 */

HANDLE Queue_Event(wQueue* queue)
{
	return queue->event;
}

/**
 * Methods
 */

/**
 * Removes all objects from the Queue.
 */

void Queue_Clear(wQueue* queue)
{
	int index;

	if (queue->synchronized)
		EnterCriticalSection(&queue->lock);

	for (index = queue->head; index != queue->tail; index = (index + 1) % queue->capacity)
	{
		if (queue->object.fnObjectFree)
			queue->object.fnObjectFree(queue->array[index]);

		queue->array[index] = NULL;
	}

	queue->size = 0;
	queue->head = queue->tail = 0;

	if (queue->synchronized)
		LeaveCriticalSection(&queue->lock);
}

/**
 * Determines whether an element is in the Queue.
 */

BOOL Queue_Contains(wQueue* queue, void* obj)
{
	int index;
	BOOL found = FALSE;

	if (queue->synchronized)
		EnterCriticalSection(&queue->lock);

	for (index = 0; index < queue->tail; index++)
	{
		if (queue->array[index] == obj)
		{
			found = TRUE;
			break;
		}
	}

	if (queue->synchronized)
		LeaveCriticalSection(&queue->lock);

	return found;
}

/**
 * Adds an object to the end of the Queue.
 */

void Queue_Enqueue(wQueue* queue, void* obj)
{
	if (queue->synchronized)
		EnterCriticalSection(&queue->lock);

	if (queue->size == queue->capacity)
	{
		int old_capacity;
		int new_capacity;

		old_capacity = queue->capacity;
		new_capacity = queue->capacity * queue->growthFactor;

		queue->capacity = new_capacity;
		queue->array = (void**) realloc(queue->array, sizeof(void*) * queue->capacity);
		ZeroMemory(&(queue->array[old_capacity]), old_capacity * sizeof(void*));

		if (queue->tail < old_capacity)
		{
			CopyMemory(&(queue->array[old_capacity]), queue->array, queue->tail * sizeof(void*));
			queue->tail += old_capacity;
		}
	}

	queue->array[queue->tail] = obj;
	queue->tail = (queue->tail + 1) % queue->capacity;
	queue->size++;

	SetEvent(queue->event);

	if (queue->synchronized)
		LeaveCriticalSection(&queue->lock);
}

/**
 * Removes and returns the object at the beginning of the Queue.
 */

void* Queue_Dequeue(wQueue* queue)
{
	void* obj = NULL;

	if (queue->synchronized)
		EnterCriticalSection(&queue->lock);

	if (queue->size > 0)
	{
		obj = queue->array[queue->head];
		queue->array[queue->head] = NULL;
		queue->head = (queue->head + 1) % queue->capacity;
		queue->size--;
	}

	if (queue->size < 1)
		ResetEvent(queue->event);

	if (queue->synchronized)
		LeaveCriticalSection(&queue->lock);

	return obj;
}

/**
 * Returns the object at the beginning of the Queue without removing it.
 */

void* Queue_Peek(wQueue* queue)
{
	void* obj = NULL;

	if (queue->synchronized)
		EnterCriticalSection(&queue->lock);

	if (queue->size > 0)
		obj = queue->array[queue->head];

	if (queue->synchronized)
		LeaveCriticalSection(&queue->lock);

	return obj;
}

/**
 * Construction, Destruction
 */

wQueue* Queue_New(BOOL synchronized, int capacity, int growthFactor)
{
	wQueue* queue = NULL;

	queue = (wQueue*) malloc(sizeof(wQueue));

	if (queue)
	{
		queue->head = 0;
		queue->tail = 0;
		queue->size = 0;

		queue->capacity = 32;
		queue->growthFactor = 2;

		queue->synchronized = synchronized;

		if (capacity > 0)
			queue->capacity = capacity;

		if (growthFactor > 0)
			queue->growthFactor = growthFactor;

		queue->array = (void**) malloc(sizeof(void*) * queue->capacity);
		ZeroMemory(queue->array, sizeof(void*) * queue->capacity);

		InitializeCriticalSectionAndSpinCount(&queue->lock, 4000);
		queue->event = CreateEvent(NULL, TRUE, FALSE, NULL);

		ZeroMemory(&queue->object, sizeof(wObject));
	}

	return queue;
}

void Queue_Free(wQueue* queue)
{
	Queue_Clear(queue);

	CloseHandle(queue->event);
	DeleteCriticalSection(&queue->lock);
	free(queue->array);
	free(queue);
}
