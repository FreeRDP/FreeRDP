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

/**
 * Gets a buffer of at least the specified size from the pool.
 */

void* BufferPool_Take(wBufferPool* pool, int bufferSize)
{
	void* buffer = NULL;

	if (pool->synchronized)
		EnterCriticalSection(&pool->lock);

	if (pool->fixedSize)
	{
		if (pool->size > 0)
			buffer = pool->array[--(pool->size)];

		if (!buffer)
		{
			if (pool->alignment)
				buffer = _aligned_malloc(pool->fixedSize, pool->alignment);
			else
				buffer = malloc(pool->fixedSize);
		}
	}
	else
	{
		fprintf(stderr, "Variable-size BufferPool not yet implemented\n");
	}

	if (pool->synchronized)
		LeaveCriticalSection(&pool->lock);

	return buffer;
}

/**
 * Returns a buffer to the pool.
 */

void BufferPool_Return(wBufferPool* pool, void* buffer)
{
	if (pool->synchronized)
		EnterCriticalSection(&pool->lock);

	if ((pool->size + 1) >= pool->capacity)
	{
		pool->capacity *= 2;
		pool->array = (void**) realloc(pool->array, sizeof(void*) * pool->capacity);
	}

	pool->array[(pool->size)++] = buffer;

	if (pool->synchronized)
		LeaveCriticalSection(&pool->lock);
}

/**
 * Releases the buffers currently cached in the pool.
 */

void BufferPool_Clear(wBufferPool* pool)
{
	if (pool->synchronized)
		EnterCriticalSection(&pool->lock);

	while (pool->size > 0)
	{
		(pool->size)--;

		if (pool->alignment)
			_aligned_free(pool->array[pool->size]);
		else
			free(pool->array[pool->size]);
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
			InitializeCriticalSection(&pool->lock);

		if (!pool->fixedSize)
		{
			fprintf(stderr, "Variable-size BufferPool not yet implemented\n");
		}

		pool->size = 0;
		pool->capacity = 32;
		pool->array = (void**) malloc(sizeof(void*) * pool->capacity);
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

		free(pool->array);

		free(pool);
	}
}
