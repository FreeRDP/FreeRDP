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

#include <winpr/config.h>

#include <winpr/crt.h>
#include <winpr/wlog.h>

#include <winpr/collections.h>

#include "../stream.h"

struct s_wStreamPool
{
	size_t aSize;
	size_t aCapacity;
	wStream** aArray;

	size_t uSize;
	size_t uCapacity;
	wStream** uArray;

	CRITICAL_SECTION lock;
	BOOL synchronized;
	size_t defaultSize;
};

/**
 * Lock the stream pool
 */

static INLINE void StreamPool_Lock(wStreamPool* pool)
{
	WINPR_ASSERT(pool);
	if (pool->synchronized)
		EnterCriticalSection(&pool->lock);
}

/**
 * Unlock the stream pool
 */

static INLINE void StreamPool_Unlock(wStreamPool* pool)
{
	WINPR_ASSERT(pool);
	if (pool->synchronized)
		LeaveCriticalSection(&pool->lock);
}

static BOOL StreamPool_EnsureCapacity(wStreamPool* pool, size_t count, BOOL usedOrAvailable)
{
	size_t new_cap = 0;
	size_t* cap;
	size_t* size;
	wStream*** array;

	WINPR_ASSERT(pool);

	cap = (usedOrAvailable) ? &pool->uCapacity : &pool->aCapacity;
	size = (usedOrAvailable) ? &pool->uSize : &pool->aSize;
	array = (usedOrAvailable) ? &pool->uArray : &pool->aArray;
	if (*cap == 0)
		new_cap = *size + count;
	else if (*size + count > *cap)
		new_cap = *cap * 2;
	else if ((*size + count) < *cap / 3)
		new_cap = *cap / 2;

	if (new_cap > 0)
	{
		wStream** new_arr;

		if (*cap < *size + count)
			*cap += count;

		new_arr = (wStream**)realloc(*array, sizeof(wStream*) * new_cap);
		if (!new_arr)
			return FALSE;
		*cap = new_cap;
		*array = new_arr;
	}
	return TRUE;
}

/**
 * Methods
 */

static void StreamPool_ShiftUsed(wStreamPool* pool, size_t index, INT64 count)
{
	WINPR_ASSERT(pool);
	if (count > 0)
	{
		const size_t pcount = (size_t)count;
		StreamPool_EnsureCapacity(pool, pcount, TRUE);

		MoveMemory(&pool->uArray[index + pcount], &pool->uArray[index],
		           (pool->uSize - index) * sizeof(wStream*));
		pool->uSize += pcount;
	}
	else if (count < 0)
	{
		const size_t pcount = (size_t)-count;
		if ((pool->uSize - index - pcount) > 0)
		{
			MoveMemory(&pool->uArray[index], &pool->uArray[index + pcount],
			           (pool->uSize - index - pcount) * sizeof(wStream*));
		}

		pool->uSize -= pcount;
	}
}

/**
 * Adds a used stream to the pool.
 */

static void StreamPool_AddUsed(wStreamPool* pool, wStream* s)
{
	StreamPool_EnsureCapacity(pool, 1, TRUE);
	pool->uArray[(pool->uSize)++] = s;
}

/**
 * Removes a used stream from the pool.
 */

static void StreamPool_RemoveUsed(wStreamPool* pool, wStream* s)
{
	size_t index;
	BOOL found = FALSE;

	WINPR_ASSERT(pool);
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

static void StreamPool_ShiftAvailable(wStreamPool* pool, size_t index, INT64 count)
{
	WINPR_ASSERT(pool);
	if (count > 0)
	{
		const size_t pcount = (size_t)count;

		StreamPool_EnsureCapacity(pool, pcount, FALSE);
		MoveMemory(&pool->aArray[index + pcount], &pool->aArray[index],
		           (pool->aSize - index) * sizeof(wStream*));
		pool->aSize += pcount;
	}
	else if (count < 0)
	{
		const size_t pcount = (size_t)-count;

		if ((pool->aSize - index - pcount) > 0)
		{
			MoveMemory(&pool->aArray[index], &pool->aArray[index + pcount],
			           (pool->aSize - index - pcount) * sizeof(wStream*));
		}

		pool->aSize -= pcount;
	}
}

/**
 * Gets a stream from the pool.
 */

wStream* StreamPool_Take(wStreamPool* pool, size_t size)
{
	size_t index;
	SSIZE_T foundIndex;
	wStream* s = NULL;

	StreamPool_Lock(pool);

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
		if (!s)
			goto out_fail;
	}
	else
	{
		Stream_SetPosition(s, 0);
		Stream_SetLength(s, Stream_Capacity(s));
		StreamPool_ShiftAvailable(pool, foundIndex, -1);
	}

	if (s)
	{
		s->pool = pool;
		s->count = 1;
		StreamPool_AddUsed(pool, s);
	}

out_fail:
	StreamPool_Unlock(pool);

	return s;
}

/**
 * Returns an object to the pool.
 */

static void StreamPool_Remove(wStreamPool* pool, wStream* s)
{
	StreamPool_EnsureCapacity(pool, 1, FALSE);
	Stream_EnsureValidity(s);
	for (size_t x = 0; x < pool->aSize; x++)
	{
		wStream* cs = pool->aArray[x];

		WINPR_ASSERT(cs != s);
	}
	pool->aArray[(pool->aSize)++] = s;
	StreamPool_RemoveUsed(pool, s);
}

static void StreamPool_ReleaseOrReturn(wStreamPool* pool, wStream* s)
{
	StreamPool_Lock(pool);
	if (s->count > 0)
		s->count--;
	if (s->count == 0)
		StreamPool_Remove(pool, s);
	StreamPool_Unlock(pool);
}

void StreamPool_Return(wStreamPool* pool, wStream* s)
{
	WINPR_ASSERT(pool);
	if (!s)
		return;

	StreamPool_Lock(pool);
	StreamPool_Remove(pool, s);
	StreamPool_Unlock(pool);
}

/**
 * Increment stream reference count
 */

void Stream_AddRef(wStream* s)
{
	WINPR_ASSERT(s);
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
	WINPR_ASSERT(s);
	if (s->pool)
		StreamPool_ReleaseOrReturn(s->pool, s);
}

/**
 * Find stream in pool using pointer inside buffer
 */

wStream* StreamPool_Find(wStreamPool* pool, BYTE* ptr)
{
	size_t index;
	wStream* s = NULL;
	BOOL found = FALSE;

	StreamPool_Lock(pool);

	for (index = 0; index < pool->uSize; index++)
	{
		s = pool->uArray[index];

		if ((ptr >= Stream_Buffer(s)) && (ptr < (Stream_Buffer(s) + Stream_Capacity(s))))
		{
			found = TRUE;
			break;
		}
	}

	StreamPool_Unlock(pool);

	return (found) ? s : NULL;
}

/**
 * Releases the streams currently cached in the pool.
 */

void StreamPool_Clear(wStreamPool* pool)
{
	StreamPool_Lock(pool);

	while (pool->aSize > 0)
	{
		wStream* s = pool->aArray[--pool->aSize];
		Stream_Free(s, s->isAllocatedStream);
	}

	while (pool->uSize > 0)
	{
		wStream* s = pool->uArray[--pool->uSize];
		Stream_Free(s, s->isAllocatedStream);
	}

	StreamPool_Unlock(pool);
}

/**
 * Construction, Destruction
 */

wStreamPool* StreamPool_New(BOOL synchronized, size_t defaultSize)
{
	wStreamPool* pool = NULL;

	pool = (wStreamPool*)calloc(1, sizeof(wStreamPool));

	if (pool)
	{
		pool->synchronized = synchronized;
		pool->defaultSize = defaultSize;

		if (!StreamPool_EnsureCapacity(pool, 32, FALSE))
			goto fail;
		if (!StreamPool_EnsureCapacity(pool, 32, TRUE))
			goto fail;

		InitializeCriticalSectionAndSpinCount(&pool->lock, 4000);
	}

	return pool;
fail:
	StreamPool_Free(pool);
	return NULL;
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

char* StreamPool_GetStatistics(wStreamPool* pool, char* buffer, size_t size)
{
	WINPR_ASSERT(pool);

	if (!buffer || (size < 1))
		return NULL;
	_snprintf(buffer, size - 1,
	          "aSize    =%" PRIuz ", uSize    =%" PRIuz "aCapacity=%" PRIuz ", uCapacity=%" PRIuz,
	          pool->aSize, pool->uSize, pool->aCapacity, pool->uCapacity);
	buffer[size - 1] = '\0';
	return buffer;
}
