/**
 * WinPR: Windows Portable Runtime
 * Object Pool
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
 * Methods
 */

void StreamPool_ShiftUsed(wStreamPool* pool, int index, int count)
{
	if (count > 0)
	{
		if (pool->uSize + count > pool->uCapacity)
		{
			pool->uCapacity *= 2;
			pool->uArray = (wStream**) realloc(pool->uArray, sizeof(wStream*) * pool->uCapacity);
		}

		MoveMemory(&pool->uArray[index + count], &pool->uArray[index], (pool->uSize - index) * sizeof(wStream*));
		pool->uSize += count;
	}
	else if (count < 0)
	{
		if (pool->uSize - index + count > 0)
		{
			MoveMemory(&pool->uArray[index], &pool->uArray[index - count],
					(pool->uSize - index + count) * sizeof(wStream*));
		}

		pool->uSize += count;
	}
}

/**
 * Adds a used stream to the pool.
 */

void StreamPool_AddUsed(wStreamPool* pool, wStream* s)
{
	if ((pool->uSize + 1) >= pool->uCapacity)
	{
		pool->uCapacity *= 2;
		pool->uArray = (wStream**) realloc(pool->uArray, sizeof(wStream*) * pool->uCapacity);
	}

	pool->uArray[(pool->uSize)++] = s;
}

/**
 * Removes a used stream from the pool.
 */

void StreamPool_RemoveUsed(wStreamPool* pool, wStream* s)
{
	int index;
	BOOL found = FALSE;

	for (index = 0; index < pool->uSize; index++)
	{
		if (pool->uArray[index] == s)
		{
			found = TRUE;
			break;
		}
	}

	if (found)
		StreamPool_ShiftUsed(pool, index, -1);
}

void StreamPool_ShiftAvailable(wStreamPool* pool, int index, int count)
{
	if (count > 0)
	{
		if (pool->aSize + count > pool->aCapacity)
		{
			pool->aCapacity *= 2;
			pool->aArray = (wStream**) realloc(pool->aArray, sizeof(wStream*) * pool->aCapacity);
		}

		MoveMemory(&pool->aArray[index + count], &pool->aArray[index], (pool->aSize - index) * sizeof(wStream*));
		pool->aSize += count;
	}
	else if (count < 0)
	{
		if (pool->aSize - index + count > 0)
		{
			MoveMemory(&pool->aArray[index], &pool->aArray[index - count],
					(pool->aSize - index + count) * sizeof(wStream*));
		}

		pool->aSize += count;
	}
}

/**
 * Gets a stream from the pool.
 */

wStream* StreamPool_Take(wStreamPool* pool, size_t size)
{
	int index;
	int foundIndex;
	wStream* s = NULL;

	if (pool->synchronized)
		EnterCriticalSection(&pool->lock);

	if (size == 0)
		size = pool->defaultSize;

	foundIndex = -1;

	for (index = 0; index < pool->aSize; index++)
	{
		s = pool->aArray[index];

		if (Stream_Capacity(s) >= size)
		{
			foundIndex = index;
			break;
		}
	}

	if (foundIndex < 0)
	{
		s = Stream_New(NULL, size);
	}
	else
	{
		StreamPool_ShiftAvailable(pool, foundIndex, -1);

		Stream_SetPosition(s, 0);
		Stream_EnsureCapacity(s, size);
	}

	s->pool = pool;
	s->count = 1;

	StreamPool_AddUsed(pool, s);

	if (pool->synchronized)
		LeaveCriticalSection(&pool->lock);

	return s;
}

/**
 * Returns an object to the pool.
 */

void StreamPool_Return(wStreamPool* pool, wStream* s)
{
	if (pool->synchronized)
		EnterCriticalSection(&pool->lock);

	if ((pool->aSize + 1) >= pool->aCapacity)
	{
		pool->aCapacity *= 2;
		pool->aArray = (wStream**) realloc(pool->aArray, sizeof(wStream*) * pool->aCapacity);
	}

	pool->aArray[(pool->aSize)++] = s;
	StreamPool_RemoveUsed(pool, s);

	if (pool->synchronized)
		LeaveCriticalSection(&pool->lock);
}

/**
 * Lock the stream pool
 */

void StreamPool_Lock(wStreamPool* pool)
{
	EnterCriticalSection(&pool->lock);
}

/**
 * Unlock the stream pool
 */

void StreamPool_Unlock(wStreamPool* pool)
{
	LeaveCriticalSection(&pool->lock);
}

/**
 * Increment stream reference count
 */

void Stream_AddRef(wStream* s)
{
	if (s->pool)
	{
		StreamPool_Lock(s->pool);
		s->count++;
		StreamPool_Unlock(s->pool);
	}
}

/**
 * Decrement stream reference count
 */

void Stream_Release(wStream* s)
{
	DWORD count;

	if (s->pool)
	{
		StreamPool_Lock(s->pool);
		count = --(s->count);
		StreamPool_Unlock(s->pool);

		if (count == 0)
			StreamPool_Return(s->pool, s);
	}
}

/**
 * Find stream in pool using pointer inside buffer
 */

wStream* StreamPool_Find(wStreamPool* pool, BYTE* ptr)
{
	int index;
	wStream* s = NULL;
	BOOL found = FALSE;

	EnterCriticalSection(&pool->lock);

	for (index = 0; index < pool->uSize; index++)
	{
		s = pool->uArray[index];

		if ((ptr >= Stream_Buffer(s)) && (ptr < (Stream_Buffer(s) + Stream_Capacity(s))))
		{
			found = TRUE;
			break;
		}
	}

	LeaveCriticalSection(&pool->lock);

	return (found) ? s : NULL;
}

/**
 * Find stream in pool and increment reference count
 */

void StreamPool_AddRef(wStreamPool* pool, BYTE* ptr)
{
	wStream* s;

	s = StreamPool_Find(pool, ptr);

	if (s)
		Stream_AddRef(s);
}

/**
 * Find stream in pool and decrement reference count
 */

void StreamPool_Release(wStreamPool* pool, BYTE* ptr)
{
	wStream* s;

	s = StreamPool_Find(pool, ptr);

	if (s)
		Stream_Release(s);
}

/**
 * Releases the streams currently cached in the pool.
 */

void StreamPool_Clear(wStreamPool* pool)
{
	if (pool->synchronized)
		EnterCriticalSection(&pool->lock);

	while (pool->aSize > 0)
	{
		(pool->aSize)--;
		Stream_Free(pool->aArray[pool->aSize], TRUE);
	}

	if (pool->synchronized)
		LeaveCriticalSection(&pool->lock);
}

/**
 * Construction, Destruction
 */

wStreamPool* StreamPool_New(BOOL synchronized, size_t defaultSize)
{
	wStreamPool* pool = NULL;

	pool = (wStreamPool*) calloc(1, sizeof(wStreamPool));

	if (pool)
	{
		pool->synchronized = synchronized;
		pool->defaultSize = defaultSize;

		pool->aSize = 0;
		pool->aCapacity = 32;
		pool->aArray = (wStream**) calloc(pool->aCapacity, sizeof(wStream*));

		if (!pool->aArray)
			return NULL;

		pool->uSize = 0;
		pool->uCapacity = 32;
		pool->uArray = (wStream**) calloc(pool->uCapacity, sizeof(wStream*));

		if (!pool->uArray)
			return NULL;

		InitializeCriticalSectionAndSpinCount(&pool->lock, 4000);
	}

	return pool;
}

void StreamPool_Free(wStreamPool* pool)
{
	if (pool)
	{
		StreamPool_Clear(pool);

		DeleteCriticalSection(&pool->lock);

		free(pool->aArray);
		free(pool->uArray);

		free(pool);
	}
}
