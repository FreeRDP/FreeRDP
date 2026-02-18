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
	uintptr_t* array;
	CRITICAL_SECTION lock;
	HANDLE event;

	wObject object;
	BOOL haveLock;

	BYTE padding2[4];
};

static inline void* uptr2void(uintptr_t ptr)
{
	return (void*)ptr;
}

static inline uintptr_t void2uptr(const void* ptr)
{
	return (uintptr_t)ptr;
}

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
	size_t ret = 0;

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
	Queue_Lock(queue);

	for (size_t index = queue->head; index != queue->tail; index = (index + 1) % queue->capacity)
	{
		if (queue->object.fnObjectFree)
		{
			void* obj = uptr2void(queue->array[index]);
			queue->object.fnObjectFree(obj);
		}

		queue->array[index] = 0;
	}

	queue->size = 0;
	queue->head = queue->tail = 0;
	(void)ResetEvent(queue->event);
	Queue_Unlock(queue);
}

/**
 * Determines whether an element is in the Queue.
 */

BOOL Queue_Contains(wQueue* queue, const void* obj)
{
	BOOL found = FALSE;

	Queue_Lock(queue);

	for (size_t index = 0; index < queue->tail; index++)
	{
		void* ptr = uptr2void(queue->array[index]);
		if (queue->object.fnObjectEquals(ptr, obj))
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

	if (queue->size + count > queue->capacity)
	{
		if (queue->growthFactor > SIZE_MAX / 32ull)
			return FALSE;
		if (queue->size > SIZE_MAX - count)
			return FALSE;

		const size_t increment = 32ull * queue->growthFactor;
		const size_t old_capacity = queue->capacity;
		const size_t required = queue->size + count;
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
			ZeroMemory(queue->array, batch * sizeof(uintptr_t));

			/* Tail is decremented. if the whole thing is appended
			 * just move the existing tail by old_capacity */
			if (tocopy < slots)
				queue->tail += old_capacity;
			else
			{
				const size_t movesize = (queue->tail - batch) * sizeof(uintptr_t);
				memmove_s(queue->array, queue->tail * sizeof(uintptr_t), &queue->array[batch],
				          movesize);
				ZeroMemory(&queue->array[batch], movesize);
				queue->tail -= batch;
			}
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
		queue->array[queue->tail] = void2uptr(queue->object.fnObjectNew(obj));
	else
		queue->array[queue->tail] = void2uptr(obj);

	queue->tail = (queue->tail + 1) % queue->capacity;

	{
		const BOOL signalSet = queue->size == 0;
		queue->size++;

		if (signalSet)
			(void)SetEvent(queue->event);
	}
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
		obj = uptr2void(queue->array[queue->head]);
		queue->array[queue->head] = 0;
		queue->head = (queue->head + 1) % queue->capacity;
		queue->size--;
	}

	if (queue->size < 1)
		(void)ResetEvent(queue->event);

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
		obj = uptr2void(queue->array[queue->head]);

	Queue_Unlock(queue);

	return obj;
}

void Queue_Discard(wQueue* queue)
{
	void* obj = NULL;

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
	wQueue* queue = (wQueue*)calloc(1, sizeof(wQueue));

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

	{
		wObject* obj = Queue_Object(queue);
		obj->fnObjectEquals = default_queue_equals;
	}
	return queue;
fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	Queue_Free(queue);
	WINPR_PRAGMA_DIAG_POP
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
	(void)CloseHandle(queue->event);
	free(queue->array);
	free(queue);
}
