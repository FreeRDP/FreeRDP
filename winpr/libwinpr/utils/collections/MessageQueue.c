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

#include <winpr/config.h>

#include <winpr/crt.h>
#include <winpr/sysinfo.h>
#include <winpr/assert.h>

#include <winpr/collections.h>

struct s_wMessageQueue
{
	size_t head;
	size_t tail;
	size_t size;
	size_t capacity;
	BOOL closed;
	wMessage* array;
	CRITICAL_SECTION lock;
	HANDLE event;

	wObject object;
};

/**
 * Message Queue inspired from Windows:
 * http://msdn.microsoft.com/en-us/library/ms632590/
 */

/**
 * Properties
 */

wObject* MessageQueue_Object(wMessageQueue* queue)
{
	WINPR_ASSERT(queue);
	return &queue->object;
}

/**
 * Gets an event which is set when the queue is non-empty
 */

HANDLE MessageQueue_Event(wMessageQueue* queue)
{
	WINPR_ASSERT(queue);
	return queue->event;
}

/**
 * Gets the queue size
 */

size_t MessageQueue_Size(wMessageQueue* queue)
{
	WINPR_ASSERT(queue);
	EnterCriticalSection(&queue->lock);
	const size_t ret = queue->size;
	LeaveCriticalSection(&queue->lock);
	return ret;
}

size_t MessageQueue_Capacity(wMessageQueue* queue)
{
	WINPR_ASSERT(queue);
	EnterCriticalSection(&queue->lock);
	const size_t ret = queue->capacity;
	LeaveCriticalSection(&queue->lock);
	return ret;
}

/**
 * Methods
 */

BOOL MessageQueue_Wait(wMessageQueue* queue)
{
	BOOL status = FALSE;

	WINPR_ASSERT(queue);
	if (WaitForSingleObject(queue->event, INFINITE) == WAIT_OBJECT_0)
		status = TRUE;

	return status;
}

static BOOL MessageQueue_EnsureCapacity(wMessageQueue* queue, size_t count)
{
	const size_t increment = 128;
	WINPR_ASSERT(queue);

	const size_t required = queue->size + count;
	// check for overflow
	if ((required < queue->size) || (required < count) ||
	    (required > (SIZE_MAX - increment) / sizeof(wMessage)))
		return FALSE;

	if (required > queue->capacity)
	{
		const size_t old_capacity = queue->capacity;
		const size_t new_capacity = required + increment;

		wMessage* new_arr = (wMessage*)realloc(queue->array, sizeof(wMessage) * new_capacity);
		if (!new_arr)
			return FALSE;
		queue->array = new_arr;
		queue->capacity = new_capacity;
		ZeroMemory(&(queue->array[old_capacity]), (new_capacity - old_capacity) * sizeof(wMessage));

		/* rearrange wrapped entries:
		 * fill up the newly available space and move tail
		 * back by the amount of elements that have been moved to the newly
		 * allocated space.
		 */
		if (queue->tail <= queue->head)
		{
			size_t tocopy = queue->tail;
			size_t slots = new_capacity - old_capacity;
			const size_t batch = (tocopy < slots) ? tocopy : slots;
			CopyMemory(&(queue->array[old_capacity]), queue->array, batch * sizeof(wMessage));

			/* Tail is decremented. if the whole thing is appended
			 * just move the existing tail by old_capacity */
			if (tocopy < slots)
			{
				ZeroMemory(queue->array, batch * sizeof(wMessage));
				queue->tail += old_capacity;
			}
			else
			{
				const size_t remain = queue->tail - batch;
				const size_t movesize = remain * sizeof(wMessage);
				memmove_s(queue->array, queue->tail * sizeof(wMessage), &queue->array[batch],
				          movesize);

				const size_t zerooffset = remain;
				const size_t zerosize = (queue->tail - remain) * sizeof(wMessage);
				ZeroMemory(&queue->array[zerooffset], zerosize);
				queue->tail -= batch;
			}
		}
	}

	return TRUE;
}

BOOL MessageQueue_Dispatch(wMessageQueue* queue, const wMessage* message)
{
	wMessage* dst = nullptr;
	BOOL ret = FALSE;
	WINPR_ASSERT(queue);

	if (!message)
		return FALSE;

	WINPR_ASSERT(queue);
	EnterCriticalSection(&queue->lock);

	if (queue->closed)
		goto out;

	if (!MessageQueue_EnsureCapacity(queue, 1))
		goto out;

	dst = &(queue->array[queue->tail]);
	*dst = *message;
	dst->time = GetTickCount64();

	queue->tail = (queue->tail + 1) % queue->capacity;
	queue->size++;

	if (queue->size > 0)
		(void)SetEvent(queue->event);

	if (message->id == WMQ_QUIT)
		queue->closed = TRUE;

	ret = TRUE;
out:
	LeaveCriticalSection(&queue->lock);
	return ret;
}

BOOL MessageQueue_Post(wMessageQueue* queue, void* context, UINT32 type, void* wParam, void* lParam)
{
	wMessage message = WINPR_C_ARRAY_INIT;

	message.context = context;
	message.id = type;
	message.wParam = wParam;
	message.lParam = lParam;
	message.Free = nullptr;

	return MessageQueue_Dispatch(queue, &message);
}

BOOL MessageQueue_PostQuit(wMessageQueue* queue, int nExitCode)
{
	return MessageQueue_Post(queue, nullptr, WMQ_QUIT, (void*)(size_t)nExitCode, nullptr);
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
			(void)ResetEvent(queue->event);

		status = (message->id != WMQ_QUIT) ? 1 : 0;
	}

	LeaveCriticalSection(&queue->lock);

	return status;
}

int MessageQueue_Peek(wMessageQueue* queue, wMessage* message, BOOL remove)
{
	int status = 0;

	WINPR_ASSERT(queue);
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
				(void)ResetEvent(queue->event);
		}
	}

	LeaveCriticalSection(&queue->lock);

	return status;
}

/**
 * Construction, Destruction
 */

wMessageQueue* MessageQueue_New(const wObject* callback)
{
	wMessageQueue* queue = nullptr;

	queue = (wMessageQueue*)calloc(1, sizeof(wMessageQueue));
	if (!queue)
		return nullptr;

	if (!InitializeCriticalSectionAndSpinCount(&queue->lock, 4000))
		goto fail;

	if (!MessageQueue_EnsureCapacity(queue, 32))
		goto fail;

	queue->event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!queue->event)
		goto fail;

	if (callback)
		queue->object = *callback;

	return queue;

fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	MessageQueue_Free(queue);
	WINPR_PRAGMA_DIAG_POP
	return nullptr;
}

void MessageQueue_Free(wMessageQueue* queue)
{
	if (!queue)
		return;

	if (queue->event)
		MessageQueue_Clear(queue);

	(void)CloseHandle(queue->event);
	DeleteCriticalSection(&queue->lock);

	free(queue->array);
	free(queue);
}

int MessageQueue_Clear(wMessageQueue* queue)
{
	int status = 0;

	WINPR_ASSERT(queue);
	WINPR_ASSERT(queue->event);

	EnterCriticalSection(&queue->lock);

	while (queue->size > 0)
	{
		wMessage* msg = &(queue->array[queue->head]);

		/* Free resources of message. */
		if (queue->object.fnObjectUninit)
			queue->object.fnObjectUninit(msg);
		if (queue->object.fnObjectFree)
			queue->object.fnObjectFree(msg);

		ZeroMemory(msg, sizeof(wMessage));

		queue->head = (queue->head + 1) % queue->capacity;
		queue->size--;
	}
	(void)ResetEvent(queue->event);
	queue->closed = FALSE;

	LeaveCriticalSection(&queue->lock);

	return status;
}
