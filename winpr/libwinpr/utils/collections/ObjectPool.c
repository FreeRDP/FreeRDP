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
 * C Object Pool similar to C# BufferManager Class:
 * http://msdn.microsoft.com/en-us/library/ms405814.aspx
 */

/**
 * Methods
 */

/**
 * Gets an object from the pool.
 */

void* ObjectPool_Take(wObjectPool* pool)
{
	void* obj = NULL;

	if (pool->synchronized)
		WaitForSingleObject(pool->mutex, INFINITE);

	if (pool->size > 0)
		obj = pool->array[--(pool->size)];

	if (!obj)
	{
		if (pool->object.fnObjectNew)
			obj = pool->object.fnObjectNew();
	}

	if (pool->synchronized)
		ReleaseMutex(pool->mutex);

	return obj;
}

/**
 * Returns an object to the pool.
 */

void ObjectPool_Return(wObjectPool* pool, void* obj)
{
	if (pool->synchronized)
		WaitForSingleObject(pool->mutex, INFINITE);

	if ((pool->size + 1) >= pool->capacity)
	{
		pool->capacity *= 2;
		pool->array = (void**) realloc(pool->array, sizeof(void*) * pool->capacity);
	}

	pool->array[(pool->size)++] = obj;

	if (pool->synchronized)
		ReleaseMutex(pool->mutex);
}

/**
 * Releases the buffers currently cached in the pool.
 */

void ObjectPool_Clear(wObjectPool* pool)
{
	if (pool->synchronized)
		WaitForSingleObject(pool->mutex, INFINITE);

	while (pool->size > 0)
	{
		(pool->size)--;

		if (pool->object.fnObjectFree)
			pool->object.fnObjectFree(pool->array[pool->size]);
	}

	if (pool->synchronized)
		ReleaseMutex(pool->mutex);
}

/**
 * Construction, Destruction
 */

wObjectPool* ObjectPool_New(BOOL synchronized)
{
	wObjectPool* pool = NULL;

	pool = (wObjectPool*) malloc(sizeof(wObjectPool));

	if (pool)
	{
		ZeroMemory(pool, sizeof(wObjectPool));

		pool->synchronized = synchronized;

		if (pool->synchronized)
			pool->mutex = CreateMutex(NULL, FALSE, NULL);

		pool->size = 0;
		pool->capacity = 32;
		pool->array = (void**) malloc(sizeof(void*) * pool->capacity);
	}

	return pool;
}

void ObjectPool_Free(wObjectPool* pool)
{
	if (pool)
	{
		ObjectPool_Clear(pool);

		if (pool->synchronized)
			CloseHandle(pool->mutex);

		free(pool->array);

		free(pool);
	}
}
