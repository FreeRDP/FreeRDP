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

#include <winpr/config.h>

#include <winpr/crt.h>
#include <winpr/assert.h>

#include <winpr/collections.h>

struct s_wQueue
{
	size_t capacity;
	size_t growthFactor;
	BOOL synchronized;

	BYTE padding[4];

	size_t head;
	size_t tail;
	size_t size;
	void** array;
	CRITICAL_SECTION lock;
	HANDLE event;

	wObject object;
	BOOL haveLock;

	BYTE padding2[4];
};

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

size_t Queue_Count(wQueue* queue)
{
	size_t ret;

	Queue_Lock(queue);

	ret = queue->size;

	Queue_Unlock(queue);

	return ret;
}

/**
 * Lock access to the ArrayList
 */

void Queue_Lock(wQueue* queue)
{
	WINPR_ASSERT(queue);
	if (queue->synchronized)
		EnterCriticalSection(&queue->lock);
}

/**
 * Unlock access to the ArrayList
 */

void Queue_Unlock(wQueue* queue)
{
	WINPR_ASSERT(queue);
	if (queue->synchronized)
		LeaveCriticalSection(&queue->lock);
}

/**
 * Gets an event which is set when the queue is non-empty
 */

HANDLE Queue_Event(wQueue* queue)
{
	WINPR_ASSERT(queue);
	return queue->event;
}

wObject* Queue_Object(wQueue* queue)
{
	WINPR_ASSERT(queue);
	return &queue->object;
}

/**
 * Methods
 */

/**
 * Removes all objects from the Queue.
 */

void Queue_Clear(wQueue* queue)
{
	size_t index;

	Queue_Lock(queue);

	for (index = queue->head; index != queue->tail; index = (index + 1) % queue->capacity)
	{
		if (queue->object.fnObjectFree)
			queue->object.fnObjectFree(queue->array[index]);

		queue->array[index] = NULL;
	}

	queue->size = 0;
	queue->head = queue->tail = 0;
	ResetEvent(queue->event);
	Queue_Unlock(queue);
}

/**
 * Determines whether an element is in the Queue.
 */

BOOL Queue_Contains(wQueue* queue, const void* obj)
{
	size_t index;
	BOOL found = FALSE;

	Queue_Lock(queue);

	for (index = 0; index < queue->tail; index++)
	{
		if (queue->object.fnObjectEquals(queue->array[index], obj))
		{
			found = TRUE;
			break;
		}
	}

	Queue_Unlock(queue);

	return found;
}

static BOOL Queue_EnsureCapacity(wQueue* queue, size_t count)
{
	WINPR_ASSERT(queue);

	if (queue->size + count >= queue->capacity)
	{
		const size_t old_capacity = queue->capacity;
		size_t new_capacity = queue->capacity * queue->growthFactor;
		void** newArray;
		if (new_capacity < queue->size + count)
			new_capacity = queue->size + count;
		newArray = (void**)realloc(queue->array, sizeof(void*) * new_capacity);

		if (!newArray)
			return FALSE;

		queue->capacity = new_capacity;
		queue->array = newArray;
		ZeroMemory(&(queue->array[old_capacity]), (new_capacity - old_capacity) * sizeof(void*));

		/* rearrange wrapped entries */
		if (queue->tail <= queue->head)
		{
			CopyMemory(&(queue->array[old_capacity]), queue->array, queue->tail * sizeof(void*));
			queue->tail += old_capacity;
		}
	}
	return TRUE;
}

/**
 * Adds an object to the end of the Queue.
 */

BOOL Queue_Enqueue(wQueue* queue, const void* obj)
{
	BOOL ret = TRUE;

	Queue_Lock(queue);

	if (!Queue_EnsureCapacity(queue, 1))
		goto out;

	if (queue->object.fnObjectNew)
		queue->array[queue->tail] = queue->object.fnObjectNew(obj);
	else
	{
		union
		{
			const void* cv;
			void* v;
		} cnv;
		cnv.cv = obj;
		queue->array[queue->tail] = cnv.v;
	}
	queue->tail = (queue->tail + 1) % queue->capacity;
	queue->size++;
	SetEvent(queue->event);
out:

	Queue_Unlock(queue);

	return ret;
}

/**
 * Removes and returns the object at the beginning of the Queue.
 */

void* Queue_Dequeue(wQueue* queue)
{
	void* obj = NULL;

	Queue_Lock(queue);

	if (queue->size > 0)
	{
		obj = queue->array[queue->head];
		queue->array[queue->head] = NULL;
		queue->head = (queue->head + 1) % queue->capacity;
		queue->size--;
	}

	if (queue->size < 1)
		ResetEvent(queue->event);

	Queue_Unlock(queue);

	return obj;
}

/**
 * Returns the object at the beginning of the Queue without removing it.
 */

void* Queue_Peek(wQueue* queue)
{
	void* obj = NULL;

	Queue_Lock(queue);

	if (queue->size > 0)
		obj = queue->array[queue->head];

	Queue_Unlock(queue);

	return obj;
}

void Queue_Discard(wQueue* queue)
{
	void* obj;

	Queue_Lock(queue);
	obj = Queue_Dequeue(queue);

	if (queue->object.fnObjectFree)
		queue->object.fnObjectFree(obj);
	Queue_Unlock(queue);
}

static BOOL default_queue_equals(const void* obj1, const void* obj2)
{
	return (obj1 == obj2);
}

/**
 * Construction, Destruction
 */

wQueue* Queue_New(BOOL synchronized, SSIZE_T capacity, SSIZE_T growthFactor)
{
	wObject* obj;
	wQueue* queue = NULL;
	queue = (wQueue*)calloc(1, sizeof(wQueue));

	if (!queue)
		return NULL;

	queue->synchronized = synchronized;

	queue->growthFactor = 2;
	if (growthFactor > 0)
		queue->growthFactor = (size_t)growthFactor;

	if (capacity <= 0)
		capacity = 32;
	if (!InitializeCriticalSectionAndSpinCount(&queue->lock, 4000))
		goto fail;
	queue->haveLock = TRUE;
	if (!Queue_EnsureCapacity(queue, (size_t)capacity))
		goto fail;

	queue->event = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!queue->event)
		goto fail;

	obj = Queue_Object(queue);
	obj->fnObjectEquals = default_queue_equals;

	return queue;
fail:
	Queue_Free(queue);
	return NULL;
}

void Queue_Free(wQueue* queue)
{
	if (!queue)
		return;

	if (queue->haveLock)
	{
		Queue_Clear(queue);
		DeleteCriticalSection(&queue->lock);
	}
	CloseHandle(queue->event);
	free(queue->array);
	free(queue);
}
