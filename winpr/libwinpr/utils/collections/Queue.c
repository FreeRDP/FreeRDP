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
	if (queue->bSynchronized)
	{

	}

	return 0;
}

/**
 * Gets a value indicating whether access to the Queue is synchronized (thread safe).
 */

BOOL Queue_IsSynchronized(wQueue* queue)
{
	return queue->bSynchronized;
}

/**
 * Methods
 */

/**
 * Removes all objects from the Queue.
 */

void Queue_Clear(wQueue* queue)
{
	if (queue->bSynchronized)
	{

	}
}

/**
 * Determines whether an element is in the Queue.
 */

BOOL Queue_Contains(wQueue* queue, void* obj)
{
	if (queue->bSynchronized)
	{

	}

	return FALSE;
}

/**
 * Adds an object to the end of the Queue.
 */

void Queue_Enqueue(wQueue* queue, void* obj)
{
	if (queue->bSynchronized)
	{

	}
}

/**
 * Removes and returns the object at the beginning of the Queue.
 */

void* Queue_Dequeue(wQueue* queue)
{
	if (queue->bSynchronized)
	{

	}

	return NULL;
}

/**
 * Returns the object at the beginning of the Queue without removing it.
 */

void* Queue_Peek(wQueue* queue)
{
	if (queue->bSynchronized)
	{

	}

	return NULL;
}

/**
 * Construction, Destruction
 */

wQueue* Queue_New(BOOL bSynchronized)
{
	wQueue* queue = NULL;

	queue = (wQueue*) malloc(sizeof(wQueue));

	if (queue)
	{
		queue->bSynchronized = bSynchronized;
	}

	return queue;
}

void Queue_Free(wQueue* queue)
{
	free(queue);
}
