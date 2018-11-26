/**
 * WinPR: Windows Portable Runtime
 * Buffer Pool
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

#include <assert.h>

#include <winpr/crt.h>

#include <winpr/collections.h>

/**
 * C equivalent of the C# BufferManager Class:
 * http://msdn.microsoft.com/en-us/library/ms405814.aspx
 */

struct _wBufferPoolItem
{
	size_t size;
	void* buffer;
	BOOL used;
};
typedef struct _wBufferPoolItem wBufferPoolItem;

struct _wBufferPool
{
	size_t fixedSize;
	DWORD alignment;
	BOOL synchronized;
	CRITICAL_SECTION lock;

	size_t size;
	size_t capacity;
	void** array;

	size_t uSize;
	size_t aSize;
	size_t uCapacity;
	wBufferPoolItem* uArray;
};

/**
 * Methods
 */

/**
 * Get the buffer pool size
 */

size_t BufferPool_GetPoolSize(wBufferPool* pool)
{
	size_t size;
	assert(pool);

	if (pool->synchronized)
		EnterCriticalSection(&pool->lock);

	if (pool->fixedSize)
	{
		/* fixed size buffers */
		size = pool->size;
	}
	else
	{
		/* variable size buffers */
		size = pool->uSize - pool->aSize;
	}

	if (pool->synchronized)
		LeaveCriticalSection(&pool->lock);

	return size;
}

/**
 * Get the size of a pooled buffer
 */

SSIZE_T BufferPool_GetBufferSize(wBufferPool* pool, const void* buffer)
{
	size_t size = 0;
	size_t index = 0;
	BOOL found = FALSE;
	assert(pool);

	if (pool->synchronized)
		EnterCriticalSection(&pool->lock);

	if (pool->fixedSize)
	{
		/* fixed size buffers */
		size = pool->fixedSize;
		found = TRUE;
	}
	else
	{
		/* variable size buffers */
		for (index = 0; index < pool->uSize; index++)
		{
			wBufferPoolItem* item = &pool->uArray[index];

			if (item->buffer == buffer)
			{
				assert(item->used);
				size = item->size;
				found = TRUE;
				break;
			}
		}
	}

	if (pool->synchronized)
		LeaveCriticalSection(&pool->lock);

	return (found) ? (SSIZE_T)size : -1;
}

/**
 * Gets a buffer of at least the specified size from the pool.
 */

void* BufferPool_Take(wBufferPool* pool, size_t size)
{
	size_t index;
	size_t maxSize;
	size_t maxIndex;
	size_t foundIndex = 0;
	BOOL found = FALSE;
	void* buffer = NULL;
	assert(pool);

	if (pool->synchronized)
		EnterCriticalSection(&pool->lock);

	if (pool->fixedSize)
	{
		/* fixed size buffers */
		if (pool->size > 0)
			buffer = pool->array[--(pool->size)];

		if (!buffer)
		{
			if (pool->alignment)
				buffer = _aligned_malloc(pool->fixedSize, pool->alignment);
			else
				buffer = malloc(pool->fixedSize);
		}

		if (!buffer)
			goto out_error;
	}
	else
	{
		/* variable size buffers */
		maxSize = 0;
		maxIndex = 0;

		if (size < 1)
			size = pool->fixedSize;

		for (index = 0; index < pool->uSize; index++)
		{
			wBufferPoolItem* item = &pool->uArray[index];

			if (item->used)
				continue;

			if (item->size > maxSize)
			{
				maxIndex = index;
				maxSize = item->size;
			}

			if (item->size >= size)
			{
				foundIndex = index;
				found = TRUE;
				break;
			}
		}

		if (!found && maxSize)
		{
			foundIndex = maxIndex;
			found = TRUE;
		}

		if (!found)
		{
			if (!size)
				buffer = NULL;
			else
			{
				if (pool->alignment)
					buffer = _aligned_malloc(size, pool->alignment);
				else
					buffer = malloc(size);

				if (!buffer)
					goto out_error;
			}
		}
		else
		{
			buffer = pool->uArray[foundIndex].buffer;
			pool->aSize++;

			if (maxSize < size)
			{
				void* newBuffer;

				if (pool->alignment)
					newBuffer = _aligned_realloc(buffer, size, pool->alignment);
				else
					newBuffer = realloc(buffer, size);

				if (!newBuffer)
					goto out_error;

				buffer = newBuffer;
			}
		}

		if (!buffer)
			goto out_error;

		if (pool->uSize + 1 > pool->uCapacity)
		{
			size_t newUCapacity = pool->uCapacity * 2;
			wBufferPoolItem* newUArray;

			if (pool->uCapacity > SIZE_MAX / 2)
				goto out_error;

			newUArray = (wBufferPoolItem*)realloc(pool->uArray, sizeof(wBufferPoolItem) * newUCapacity);

			if (!newUArray)
				goto out_error;

			memset(&pool->uArray[pool->uCapacity], 0,
			       sizeof(wBufferPoolItem) * (newUCapacity - pool->uCapacity));
			pool->uCapacity = newUCapacity;
			pool->uArray = newUArray;
		}

		pool->uArray[pool->uSize].buffer = buffer;
		pool->uArray[pool->uSize].size = size;
		pool->uArray[pool->uSize].used = TRUE;
		pool->aSize++;
		pool->uSize++;
	}

	if (pool->synchronized)
		LeaveCriticalSection(&pool->lock);

	return buffer;
out_error:

	if (pool->alignment)
		_aligned_free(buffer);
	else
		free(buffer);

	if (pool->synchronized)
		LeaveCriticalSection(&pool->lock);

	return NULL;
}

/**
 * Returns a buffer to the pool.
 */

BOOL BufferPool_Return(wBufferPool* pool, void* buffer)
{
	size_t index = 0;
	assert(pool);

	if (pool->synchronized)
		EnterCriticalSection(&pool->lock);

	if (pool->fixedSize)
	{
		/* fixed size buffers */
		if ((pool->size + 1) >= pool->capacity)
		{
			size_t newCapacity = pool->capacity * 2;
			void** newArray;

			if (pool->capacity > SIZE_MAX / 2)
				goto out_error;

			newArray = (void**)realloc(pool->array, sizeof(void*) * newCapacity);

			if (!newArray)
				goto out_error;

			pool->capacity = newCapacity;
			pool->array = newArray;
		}

		pool->array[(pool->size)++] = buffer;
	}
	else
	{
		/* variable size buffers */
		for (index = 0; index < pool->uSize; index++)
		{
			if (pool->uArray[index].buffer == buffer)
			{
				assert(pool->uArray[index].used);
				pool->uArray[index].used = FALSE;
				pool->aSize--;
				break;
			}
		}
	}

	if (pool->synchronized)
		LeaveCriticalSection(&pool->lock);

	return TRUE;
out_error:

	if (pool->synchronized)
		LeaveCriticalSection(&pool->lock);

	return FALSE;
}

/**
 * Releases the buffers currently cached in the pool.
 */

void BufferPool_Clear(wBufferPool* pool)
{
	assert(pool);

	if (pool->synchronized)
		EnterCriticalSection(&pool->lock);

	if (pool->fixedSize)
	{
		/* fixed size buffers */
		while (pool->size > 0)
		{
			(pool->size)--;

			if (pool->alignment)
				_aligned_free(pool->array[pool->size]);
			else
				free(pool->array[pool->size]);
		}
	}
	else
	{
		/* variable size buffers */
		while (pool->uSize > 0)
		{
			(pool->uSize)--;

			if (pool->alignment)
				_aligned_free(pool->uArray[pool->uSize].buffer);
			else
				free(pool->uArray[pool->uSize].buffer);
		}
	}

	if (pool->synchronized)
		LeaveCriticalSection(&pool->lock);
}

/**
 * Construction, Destruction
 */
void BufferPool_Free(wBufferPool* pool)
{
	if (pool)
	{
		BufferPool_Clear(pool);

		if (pool->synchronized)
			DeleteCriticalSection(&pool->lock);

		if (pool->fixedSize)
		{
			/* fixed size buffers */
			free(pool->array);
		}
		else
		{
			/* variable size buffers */
			free(pool->uArray);
		}

		free(pool);
	}
}

wBufferPool* BufferPool_New(BOOL synchronized, SSIZE_T fixedSize, DWORD alignment)
{
	wBufferPool* pool = (wBufferPool*) calloc(1, sizeof(wBufferPool));

	if (pool)
	{
		if (fixedSize < 0)
			fixedSize = 0;

		pool->fixedSize = (size_t)fixedSize;
		pool->alignment = alignment;
		pool->synchronized = synchronized;

		if (pool->synchronized)
			InitializeCriticalSectionAndSpinCount(&pool->lock, 4000);

		if (pool->fixedSize)
		{
			/* fixed size buffers */
			pool->capacity = 32;
			pool->array = (void**) calloc(pool->capacity, sizeof(void*));

			if (!pool->array)
				goto out_error;
		}
		else
		{
			/* variable size buffers */
			pool->uCapacity = 32;
			pool->uArray = (wBufferPoolItem*) calloc(pool->uCapacity, sizeof(wBufferPoolItem));

			if (!pool->uArray)
				goto out_error;
		}
	}

	return pool;
out_error:
	BufferPool_Free(pool);
	return NULL;
}

