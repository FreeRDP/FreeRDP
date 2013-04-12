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

/**
 * Gets a stream from the pool.
 */

wStream* StreamPool_Take(wStreamPool* pool, size_t size)
{
	wStream* s = NULL;

	if (pool->synchronized)
		WaitForSingleObject(pool->mutex, INFINITE);

	if (pool->size > 0)
		s = pool->array[--(pool->size)];

	if (!s)
	{
		s = Stream_New(NULL, size);
		s->pool = (void*) pool;
		s->count = 1;
	}
	else
	{
		Stream_EnsureCapacity(s, size);
		Stream_Pointer(s) = Stream_Buffer(s);
		s->pool = (void*) pool;
		s->count = 1;
	}

	if (pool->synchronized)
		ReleaseMutex(pool->mutex);

	return s;
}

/**
 * Returns an object to the pool.
 */

void StreamPool_Return(wStreamPool* pool, wStream* s)
{
	if (pool->synchronized)
		WaitForSingleObject(pool->mutex, INFINITE);

	if ((pool->size + 1) >= pool->capacity)
	{
		pool->capacity *= 2;
		pool->array = (wStream**) realloc(pool->array, sizeof(wStream*) * pool->capacity);
	}

	pool->array[(pool->size)++] = s;

	if (pool->synchronized)
		ReleaseMutex(pool->mutex);
}

/**
 * Lock the stream pool
 */

void StreamPool_Lock(wStreamPool* pool)
{
	WaitForSingleObject(pool->mutex, INFINITE);
}

/**
 * Unlock the stream pool
 */

void StreamPool_Unlock(wStreamPool* pool)
{
	ReleaseMutex(pool->mutex);
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
 * Releases the streams currently cached in the pool.
 */

void StreamPool_Clear(wStreamPool* pool)
{
	if (pool->synchronized)
		WaitForSingleObject(pool->mutex, INFINITE);

	while (pool->size > 0)
	{
		(pool->size)--;
		Stream_Free(pool->array[pool->size], TRUE);
	}

	if (pool->synchronized)
		ReleaseMutex(pool->mutex);
}

/**
 * Construction, Destruction
 */

wStreamPool* StreamPool_New(BOOL synchronized)
{
	wStreamPool* pool = NULL;

	pool = (wStreamPool*) malloc(sizeof(wStreamPool));

	if (pool)
	{
		ZeroMemory(pool, sizeof(wStreamPool));

		pool->synchronized = synchronized;

		if (pool->synchronized)
			pool->mutex = CreateMutex(NULL, FALSE, NULL);

		pool->size = 0;
		pool->capacity = 32;
		pool->array = (wStream**) malloc(sizeof(wStream*) * pool->capacity);
	}

	return pool;
}

void StreamPool_Free(wStreamPool* pool)
{
	if (pool)
	{
		StreamPool_Clear(pool);

		if (pool->synchronized)
			CloseHandle(pool->mutex);

		free(pool->array);

		free(pool);
	}
}
