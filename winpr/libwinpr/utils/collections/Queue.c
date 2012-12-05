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
 * Gets a value indicating whether access to the Queue is synchronized (thread safe).
 */

BOOL Queue_IsSynchronized(wQueue* queue)
{
	return queue->synchronized;
}

/**
 * Gets an object that can be used to synchronize access to the Queue.
 */

HANDLE Queue_SyncRoot(wQueue* queue)
{
	return queue->mutex;
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
		WaitForSingleObject(queue->mutex, INFINITE);

	for (index = 0; index < queue->size; index++)
	{
		queue->array[index] = NULL;
	}

	queue->size = 0;
	queue->head = queue->tail = 0;

	if (queue->synchronized)
		ReleaseMutex(queue->mutex);
}

/**
 * Determines whether an element is in the Queue.
 */

BOOL Queue_Contains(wQueue* queue, void* obj)
{
	int index;
	BOOL found = FALSE;

	if (queue->synchronized)
		WaitForSingleObject(queue->mutex, INFINITE);

	for (index = 0; index < queue->tail; index++)
	{
		if (queue->array[index] == obj)
		{
			found = TRUE;
			break;
		}
	}

	if (queue->synchronized)
		ReleaseMutex(queue->mutex);

	return found;
}

/**
 * Adds an object to the end of the Queue.
 */

void Queue_Enqueue(wQueue* queue, void* obj)
{
	if (queue->synchronized)
		WaitForSingleObject(queue->mutex, INFINITE);

	if (queue->size == queue->capacity)
	{
		queue->capacity *= queue->growthFactor;
		queue->array = (void**) realloc(queue->array, sizeof(void*) * queue->capacity);
	}

	queue->array[queue->tail] = obj;
	queue->tail = (queue->tail + 1) % queue->capacity;
	queue->size++;

	if (queue->synchronized)
		ReleaseMutex(queue->mutex);
}

/**
 * Removes and returns the object at the beginning of the Queue.
 */

void* Queue_Dequeue(wQueue* queue)
{
	void* obj = NULL;

	if (queue->synchronized)
		WaitForSingleObject(queue->mutex, INFINITE);

	if (queue->size > 0)
	{
		obj = queue->array[queue->head];
		queue->array[queue->head] = NULL;
		queue->head = (queue->head + 1) % queue->capacity;
		queue->size--;
	}

	if (queue->synchronized)
		ReleaseMutex(queue->mutex);

	return obj;
}

/**
 * Returns the object at the beginning of the Queue without removing it.
 */

void* Queue_Peek(wQueue* queue)
{
	void* obj = NULL;

	if (queue->synchronized)
		WaitForSingleObject(queue->mutex, INFINITE);

	if (queue->size > 0)
		obj = queue->array[queue->head];

	if (queue->synchronized)
		ReleaseMutex(queue->mutex);

	return obj;
}

/**
 * Construction, Destruction
 */

wQueue* Queue_New(BOOL synchronized, int iCapacity, int iGrowthFactor)
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

		if (iCapacity > 0)
			queue->capacity = iCapacity;

		if (iGrowthFactor > 0)
			queue->growthFactor = iGrowthFactor;

		queue->array = (void**) malloc(sizeof(void*) * queue->capacity);

		queue->mutex = CreateMutex(NULL, FALSE, NULL);
	}

	return queue;
}

void Queue_Free(wQueue* queue)
{
	CloseHandle(queue->mutex);
	free(queue->array);
	free(queue);
}
