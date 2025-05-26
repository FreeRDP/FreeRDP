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
#include "../log.h"
#define TAG WINPR_TAG("utils.streampool")

struct s_StreamPoolEntry
{
#if defined(WITH_STREAMPOOL_DEBUG)
	char** msg;
	size_t lines;
#endif
	wStream* s;
};

struct s_wStreamPool
{
	size_t aSize;
	size_t aCapacity;
	struct s_StreamPoolEntry* aArray;

	size_t uSize;
	size_t uCapacity;
	struct s_StreamPoolEntry* uArray;

	CRITICAL_SECTION lock;
	BOOL synchronized;
	size_t defaultSize;
};

static void discard_entry(struct s_StreamPoolEntry* entry, BOOL discardStream)
{
	if (!entry)
		return;

#if defined(WITH_STREAMPOOL_DEBUG)
	free((void*)entry->msg);
#endif

	if (discardStream && entry->s)
		Stream_Free(entry->s, entry->s->isAllocatedStream);

	const struct s_StreamPoolEntry empty = { 0 };
	*entry = empty;
}

static struct s_StreamPoolEntry add_entry(wStream* s)
{
	struct s_StreamPoolEntry entry = { 0 };

#if defined(WITH_STREAMPOOL_DEBUG)
	void* stack = winpr_backtrace(20);
	if (stack)
		entry.msg = winpr_backtrace_symbols(stack, &entry.lines);
	winpr_backtrace_free(stack);
#endif

	entry.s = s;
	return entry;
}

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
	WINPR_ASSERT(pool);

	size_t* cap = (usedOrAvailable) ? &pool->uCapacity : &pool->aCapacity;
	size_t* size = (usedOrAvailable) ? &pool->uSize : &pool->aSize;
	struct s_StreamPoolEntry** array = (usedOrAvailable) ? &pool->uArray : &pool->aArray;

	size_t new_cap = 0;
	if (*cap == 0)
		new_cap = *size + count;
	else if (*size + count > *cap)
		new_cap = (*size + count + 2) / 2 * 3;
	else if ((*size + count) < *cap / 3)
		new_cap = *cap / 2;

	if (new_cap > 0)
	{
		struct s_StreamPoolEntry* new_arr = NULL;

		if (*cap < *size + count)
			*cap += count;

		new_arr =
		    (struct s_StreamPoolEntry*)realloc(*array, sizeof(struct s_StreamPoolEntry) * new_cap);
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

static void StreamPool_ShiftUsed(wStreamPool* pool, size_t index)
{
	WINPR_ASSERT(pool);

	const size_t pcount = 1;
	const size_t off = index + pcount;
	if (pool->uSize >= off)
	{
		for (size_t x = 0; x < pcount; x++)
		{
			struct s_StreamPoolEntry* cur = &pool->uArray[index + x];
			discard_entry(cur, FALSE);
		}
		MoveMemory(&pool->uArray[index], &pool->uArray[index + pcount],
		           (pool->uSize - index - pcount) * sizeof(struct s_StreamPoolEntry));
		pool->uSize -= pcount;
	}
}

/**
 * Adds a used stream to the pool.
 */

static void StreamPool_AddUsed(wStreamPool* pool, wStream* s)
{
	StreamPool_EnsureCapacity(pool, 1, TRUE);
	pool->uArray[pool->uSize] = add_entry(s);
	pool->uSize++;
}

/**
 * Removes a used stream from the pool.
 */

static void StreamPool_RemoveUsed(wStreamPool* pool, wStream* s)
{
	WINPR_ASSERT(pool);
	for (size_t index = 0; index < pool->uSize; index++)
	{
		struct s_StreamPoolEntry* cur = &pool->uArray[index];
		if (cur->s == s)
		{
			StreamPool_ShiftUsed(pool, index);
			break;
		}
	}
}

static void StreamPool_ShiftAvailable(wStreamPool* pool, size_t index)
{
	WINPR_ASSERT(pool);

	const size_t pcount = 1;
	const size_t off = index + pcount;
	if (pool->aSize >= off)
	{
		for (size_t x = 0; x < pcount; x++)
		{
			struct s_StreamPoolEntry* cur = &pool->aArray[index + x];
			discard_entry(cur, FALSE);
		}

		MoveMemory(&pool->aArray[index], &pool->aArray[index + pcount],
		           (pool->aSize - index - pcount) * sizeof(struct s_StreamPoolEntry));
		pool->aSize -= pcount;
	}
}

/**
 * Gets a stream from the pool.
 */

wStream* StreamPool_Take(wStreamPool* pool, size_t size)
{
	BOOL found = FALSE;
	size_t foundIndex = 0;
	wStream* s = NULL;

	StreamPool_Lock(pool);

	if (size == 0)
		size = pool->defaultSize;

	for (size_t index = 0; index < pool->aSize; index++)
	{
		struct s_StreamPoolEntry* cur = &pool->aArray[index];
		s = cur->s;

		if (Stream_Capacity(s) >= size)
		{
			found = TRUE;
			foundIndex = index;
			break;
		}
	}

	if (!found)
	{
		s = Stream_New(NULL, size);
		if (!s)
			goto out_fail;
	}
	else if (s)
	{
		Stream_SetPosition(s, 0);
		Stream_SetLength(s, Stream_Capacity(s));
		StreamPool_ShiftAvailable(pool, foundIndex);
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
		wStream* cs = pool->aArray[x].s;
		if (cs == s)
			return;
	}
	pool->aArray[(pool->aSize)++] = add_entry(s);
	StreamPool_RemoveUsed(pool, s);
}

static void StreamPool_ReleaseOrReturn(wStreamPool* pool, wStream* s)
{
	StreamPool_Lock(pool);
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
	s->count++;
}

/**
 * Decrement stream reference count
 */

void Stream_Release(wStream* s)
{
	WINPR_ASSERT(s);

	if (s->count > 0)
		s->count--;
	if (s->count == 0)
	{
		if (s->pool)
			StreamPool_ReleaseOrReturn(s->pool, s);
		else
			Stream_Free(s, TRUE);
	}
}

/**
 * Find stream in pool using pointer inside buffer
 */

wStream* StreamPool_Find(wStreamPool* pool, const BYTE* ptr)
{
	wStream* s = NULL;

	StreamPool_Lock(pool);

	for (size_t index = 0; index < pool->uSize; index++)
	{
		struct s_StreamPoolEntry* cur = &pool->uArray[index];

		if ((ptr >= Stream_Buffer(cur->s)) &&
		    (ptr < (Stream_Buffer(cur->s) + Stream_Capacity(cur->s))))
		{
			s = cur->s;
			break;
		}
	}

	StreamPool_Unlock(pool);

	return s;
}

/**
 * Releases the streams currently cached in the pool.
 */

void StreamPool_Clear(wStreamPool* pool)
{
	StreamPool_Lock(pool);

	for (size_t x = 0; x < pool->aSize; x++)
	{
		struct s_StreamPoolEntry* cur = &pool->aArray[x];
		discard_entry(cur, TRUE);
	}
	pool->aSize = 0;

	if (pool->uSize > 0)
	{
		WLog_WARN(TAG, "Clearing StreamPool, but there are %" PRIuz " streams currently in use",
		          pool->uSize);
		for (size_t x = 0; x < pool->uSize; x++)
		{
			struct s_StreamPoolEntry* cur = &pool->uArray[x];
			discard_entry(cur, TRUE);
		}
		pool->uSize = 0;
	}

	StreamPool_Unlock(pool);
}

size_t StreamPool_UsedCount(wStreamPool* pool)
{
	StreamPool_Lock(pool);
	size_t usize = pool->uSize;
	StreamPool_Unlock(pool);
	return usize;
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
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	StreamPool_Free(pool);
	WINPR_PRAGMA_DIAG_POP
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

	size_t used = 0;
	int offset = _snprintf(buffer, size - 1,
	                       "aSize    =%" PRIuz ", uSize    =%" PRIuz ", aCapacity=%" PRIuz
	                       ", uCapacity=%" PRIuz,
	                       pool->aSize, pool->uSize, pool->aCapacity, pool->uCapacity);
	if ((offset > 0) && ((size_t)offset < size))
		used += (size_t)offset;

#if defined(WITH_STREAMPOOL_DEBUG)
	StreamPool_Lock(pool);

	offset = _snprintf(&buffer[used], size - 1 - used, "\n-- dump used array take locations --\n");
	if ((offset > 0) && ((size_t)offset < size - used))
		used += (size_t)offset;
	for (size_t x = 0; x < pool->uSize; x++)
	{
		const struct s_StreamPoolEntry* cur = &pool->uArray[x];
		WINPR_ASSERT(cur->msg || (cur->lines == 0));

		for (size_t y = 0; y < cur->lines; y++)
		{
			offset = _snprintf(&buffer[used], size - 1 - used, "[%" PRIuz " | %" PRIuz "]: %s\n", x,
			                   y, cur->msg[y]);
			if ((offset > 0) && ((size_t)offset < size - used))
				used += (size_t)offset;
		}
	}

	offset = _snprintf(&buffer[used], size - 1 - used, "\n-- statistics called from --\n");
	if ((offset > 0) && ((size_t)offset < size - used))
		used += (size_t)offset;

	struct s_StreamPoolEntry entry = { 0 };
	void* stack = winpr_backtrace(20);
	if (stack)
		entry.msg = winpr_backtrace_symbols(stack, &entry.lines);
	winpr_backtrace_free(stack);

	for (size_t x = 0; x < entry.lines; x++)
	{
		const char* msg = entry.msg[x];
		offset = _snprintf(&buffer[used], size - 1 - used, "[%" PRIuz "]: %s\n", x, msg);
		if ((offset > 0) && ((size_t)offset < size - used))
			used += (size_t)offset;
	}
	free((void*)entry.msg);
	StreamPool_Unlock(pool);
#endif
	buffer[used] = '\0';
	return buffer;
}

BOOL StreamPool_WaitForReturn(wStreamPool* pool, UINT32 timeoutMS)
{
	wLog* log = WLog_Get(TAG);

	/* HACK: We disconnected the transport above, now wait without a read or write lock until all
	 * streams in use have been returned to the pool. */
	while (timeoutMS > 0)
	{
		const size_t used = StreamPool_UsedCount(pool);
		if (used == 0)
			return TRUE;
		WLog_Print(log, WLOG_DEBUG, "%" PRIuz " streams still in use, sleeping...", used);

		char buffer[4096] = { 0 };
		StreamPool_GetStatistics(pool, buffer, sizeof(buffer));
		WLog_Print(log, WLOG_TRACE, "Pool statistics: %s", buffer);

		UINT32 diff = 10;
		if (timeoutMS != INFINITE)
		{
			diff = timeoutMS > 10 ? 10 : timeoutMS;
			timeoutMS -= diff;
		}
		Sleep(diff);
	}

	return FALSE;
}
