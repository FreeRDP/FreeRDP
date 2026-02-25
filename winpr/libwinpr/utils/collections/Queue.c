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
	int ret;

	if (queue->synchronized)
		EnterCriticalSection(&queue->lock);

	ret = queue->size;

	if (queue->synchronized)
		LeaveCriticalSection(&queue->lock);

	return ret;
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
		if (queue->object.fnObjectEquals(queue->array[index], obj))
		{
			found = TRUE;
			break;
		}
	}

	if (queue->synchronized)
		LeaveCriticalSection(&queue->lock);

	return found;
}

static BOOL Queue_EnsureCapacity(wQueue* queue, size_t count)
{
	const size_t blocksize = 32ull;
	WINPR_ASSERT(queue);

	if (queue->growthFactor > SIZE_MAX / blocksize)
		return FALSE;

	const size_t increment = blocksize * queue->growthFactor;
	if (queue->size > SIZE_MAX - count)
		return FALSE;

	const size_t required = queue->size + count;
	if (required > queue->capacity)
	{
		const size_t old_capacity = queue->capacity;
		if (required > SIZE_MAX - increment)
			return FALSE;

		const size_t new_capacity = required + increment - required % increment;
		if (new_capacity > SIZE_MAX / sizeof(BYTE*))
			return FALSE;

		uintptr_t* newArray = (uintptr_t*)realloc(queue->array, sizeof(uintptr_t) * new_capacity);

		if (!newArray)
			return FALSE;

		queue->capacity = new_capacity;
		queue->array = newArray;
		ZeroMemory(&(queue->array[old_capacity]),
		           (new_capacity - old_capacity) * sizeof(uintptr_t));

		/* rearrange wrapped entries */
		if (queue->tail <= queue->head)
		{
			const size_t tocopy = queue->tail;
			const size_t slots = new_capacity - old_capacity;
			const size_t batch = (tocopy < slots) ? tocopy : slots;

			CopyMemory(&(queue->array[old_capacity]), queue->array, batch * sizeof(uintptr_t));

			/* Tail is decremented. if the whole thing is appended
			 * just move the existing tail by old_capacity */
			if (tocopy < slots)
			{
				ZeroMemory(queue->array, batch * sizeof(uintptr_t));
				queue->tail += old_capacity;
			}
			else
			{
				const size_t remain = queue->tail - batch;
				const size_t movesize = remain * sizeof(uintptr_t);
				memmove_s(queue->array, queue->tail * sizeof(uintptr_t), &queue->array[batch],
				          movesize);

				const size_t zerooffset = remain;
				const size_t zerosize = (queue->tail - remain) * sizeof(uintptr_t);
				ZeroMemory(&queue->array[zerooffset], zerosize);
				queue->tail -= batch;
			}
		}
	}
	return TRUE;
}

/**
 * Adds an object to the end of the Queue.
 */

BOOL Queue_Enqueue(wQueue* queue, void* obj)
{
	BOOL ret = TRUE;

	if (queue->synchronized)
		EnterCriticalSection(&queue->lock);

	if (!Queue_EnsureCapacity(queue, 1))
		goto out;

	queue->array[queue->tail] = obj;
	queue->tail = (queue->tail + 1) % queue->capacity;
	queue->size++;
	SetEvent(queue->event);
out:

	if (queue->synchronized)
		LeaveCriticalSection(&queue->lock);

	return ret;
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

static BOOL default_queue_equals(const void* obj1, const void* obj2)
{
	return (obj1 == obj2);
}

/**
 * Construction, Destruction
 */

wQueue* Queue_New(BOOL synchronized, int capacity, int growthFactor)
{
	wQueue* queue = NULL;
	queue = (wQueue*)calloc(1, sizeof(wQueue));

	if (!queue)
		return NULL;

	size_t scapacity = 32;
	queue->growthFactor = 2;
	queue->synchronized = synchronized;

	if (capacity > 0)
		scapacity = capacity;

	if (growthFactor > 0)
		queue->growthFactor = growthFactor;

	if (!Queue_EnsureCapacity(queue, scapacity))
		goto out_free_array;

	queue->event = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!queue->event)
		goto out_free_array;

	if (!InitializeCriticalSectionAndSpinCount(&queue->lock, 4000))
		goto out_free_event;

	queue->object.fnObjectEquals = default_queue_equals;
	return queue;
out_free_event:
	CloseHandle(queue->event);
out_free_array:
	free(queue->array);
out_free:
	free(queue);
	return NULL;
}

void Queue_Free(wQueue* queue)
{
	if (!queue)
		return;

	Queue_Clear(queue);
	CloseHandle(queue->event);
	DeleteCriticalSection(&queue->lock);
	free(queue->array);
	free(queue);
}
