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

#include <winpr/crt.h>

#include <winpr/collections.h>

/**
 * C equivalent of the C# BufferManager Class:
 * http://msdn.microsoft.com/en-us/library/ms405814.aspx
 */

/**
 * Methods
 */

BOOL BufferPool_ShiftAvailable(wBufferPool* pool, int index, int count)
{
	if (count > 0)
	{
		if (pool->aSize + count > pool->aCapacity)
		{
			wBufferPoolItem *newArray;
			int newCapacity = pool->aCapacity * 2;

			newArray = (wBufferPoolItem*) realloc(pool->aArray, sizeof(wBufferPoolItem) * newCapacity);
			if (!newArray)
				return FALSE;
			pool->aArray = newArray;
			pool->aCapacity = newCapacity;
		}

		MoveMemory(&pool->aArray[index + count], &pool->aArray[index], (pool->aSize - index) * sizeof(wBufferPoolItem));
		pool->aSize += count;
	}
	else if (count < 0)
	{
		MoveMemory(&pool->aArray[index], &pool->aArray[index - count], (pool->aSize - index) * sizeof(wBufferPoolItem));
		pool->aSize += count;
	}
	return TRUE;
}

BOOL BufferPool_ShiftUsed(wBufferPool* pool, int index, int count)
{
	if (count > 0)
	{
		if (pool->uSize + count > pool->uCapacity)
		{
			int newUCapacity = pool->uCapacity * 2;
			wBufferPoolItem *newUArray = (wBufferPoolItem *)realloc(pool->uArray, sizeof(wBufferPoolItem) *newUCapacity);
			if (!newUArray)
				return FALSE;
			pool->uCapacity = newUCapacity;
			pool->uArray = newUArray;
		}

		MoveMemory(&pool->uArray[index + count], &pool->uArray[index], (pool->uSize - index) * sizeof(wBufferPoolItem));
		pool->uSize += count;
	}
	else if (count < 0)
	{
		MoveMemory(&pool->uArray[index], &pool->uArray[index - count], (pool->uSize - index) * sizeof(wBufferPoolItem));
		pool->uSize += count;
	}
	return TRUE;
}

/**
 * Get the buffer pool size
 */

int BufferPool_GetPoolSize(wBufferPool* pool)
{
	int size;

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
		size = pool->uSize;
	}

	if (pool->synchronized)
		LeaveCriticalSection(&pool->lock);

	return size;
}

/**
 * Get the size of a pooled buffer
 */

int BufferPool_GetBufferSize(wBufferPool* pool, void* buffer)
{
	int size = 0;
	int index = 0;
	BOOL found = FALSE;

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
			if (pool->uArray[index].buffer == buffer)
			{
				size = pool->uArray[index].size;
				found = TRUE;
				break;
			}
		}
	}

	if (pool->synchronized)
		LeaveCriticalSection(&pool->lock);

	return (found) ? size : -1;
}

/**
 * Gets a buffer of at least the specified size from the pool.
 */

void* BufferPool_Take(wBufferPool* pool, int size)
{
	int index;
	int maxSize;
	int maxIndex;
	int foundIndex;
	BOOL found = FALSE;
	void* buffer = NULL;

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

		for (index = 0; index < pool->aSize; index++)
		{
			if (pool->aArray[index].size > maxSize)
			{
				maxIndex = index;
				maxSize = pool->aArray[index].size;
			}

			if (pool->aArray[index].size >= size)
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
			if (pool->alignment)
				buffer = _aligned_malloc(size, pool->alignment);
			else
				buffer = malloc(size);
		}
		else
		{
			buffer = pool->aArray[foundIndex].buffer;

			if (maxSize < size)
			{
				if (pool->alignment)
				{
					_aligned_free(buffer);
					buffer = _aligned_malloc(size, pool->alignment);
				}
				else
				{
					void *newBuffer = realloc(buffer, size);
					if (!newBuffer)
						goto out_error;

					buffer = newBuffer;
				}
			}

			if (!BufferPool_ShiftAvailable(pool, foundIndex, -1))
				goto out_error;
		}

		if (!buffer)
			goto out_error;

		if (pool->uSize + 1 > pool->uCapacity)
		{
			int newUCapacity = pool->uCapacity * 2;
			wBufferPoolItem *newUArray = (wBufferPoolItem *)realloc(pool->uArray, sizeof(wBufferPoolItem) * newUCapacity);
			if (!newUArray)
				goto out_error;

			pool->uCapacity = newUCapacity;
			pool->uArray = newUArray;
		}

		pool->uArray[pool->uSize].buffer = buffer;
		pool->uArray[pool->uSize].size = size;
		(pool->uSize)++;
	}

	if (pool->synchronized)
		LeaveCriticalSection(&pool->lock);

	return buffer;

out_error:
	if (buffer)
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
	int size = 0;
	int index = 0;
	BOOL found = FALSE;

	if (pool->synchronized)
		EnterCriticalSection(&pool->lock);

	if (pool->fixedSize)
	{
		/* fixed size buffers */

		if ((pool->size + 1) >= pool->capacity)
		{
			int newCapacity = pool->capacity * 2;
			void **newArray = (void **)realloc(pool->array, sizeof(void*) * newCapacity);
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
				found = TRUE;
				break;
			}
		}

		if (found)
		{
			size = pool->uArray[index].size;
			if (!BufferPool_ShiftUsed(pool, index, -1))
				goto out_error;
		}

		if (size)
		{
			if ((pool->aSize + 1) >= pool->aCapacity)
			{
				int newCapacity = pool->aCapacity * 2;
				wBufferPoolItem *newArray = (wBufferPoolItem*) realloc(pool->aArray, sizeof(wBufferPoolItem) * newCapacity);
				if (!newArray)
					goto out_error;

				pool->aCapacity = newCapacity;
				pool->aArray = newArray;
			}

			pool->aArray[pool->aSize].buffer = buffer;
			pool->aArray[pool->aSize].size = size;
			(pool->aSize)++;
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

		while (pool->aSize > 0)
		{
			(pool->aSize)--;

			if (pool->alignment)
				_aligned_free(pool->aArray[pool->aSize].buffer);
			else
				free(pool->aArray[pool->aSize].buffer);
		}

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

wBufferPool* BufferPool_New(BOOL synchronized, int fixedSize, DWORD alignment)
{
	wBufferPool* pool = NULL;

	pool = (wBufferPool*) malloc(sizeof(wBufferPool));

	if (pool)
	{
		pool->fixedSize = fixedSize;

		if (pool->fixedSize < 0)
			pool->fixedSize = 0;

		pool->alignment = alignment;
		pool->synchronized = synchronized;

		if (pool->synchronized)
			InitializeCriticalSectionAndSpinCount(&pool->lock, 4000);

		if (pool->fixedSize)
		{
			/* fixed size buffers */

			pool->size = 0;
			pool->capacity = 32;
			pool->array = (void**) malloc(sizeof(void*) * pool->capacity);
		}
		else
		{
			/* variable size buffers */

			pool->aSize = 0;
			pool->aCapacity = 32;
			pool->aArray = (wBufferPoolItem*) malloc(sizeof(wBufferPoolItem) * pool->aCapacity);

			pool->uSize = 0;
			pool->uCapacity = 32;
			pool->uArray = (wBufferPoolItem*) malloc(sizeof(wBufferPoolItem) * pool->uCapacity);
		}
	}

	return pool;
}

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

			free(pool->aArray);
			free(pool->uArray);
		}

		free(pool);
	}
}
