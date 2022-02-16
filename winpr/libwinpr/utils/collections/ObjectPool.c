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
#include <winpr/assert.h>

#include <winpr/collections.h>

struct s_wObjectPool
{
	size_t size;
	size_t capacity;
	void** array;
	CRITICAL_SECTION lock;
	wObject object;
	BOOL synchronized;
};

/**
 * C Object Pool similar to C# BufferManager Class:
 * http://msdn.microsoft.com/en-us/library/ms405814.aspx
 */

/**
 * Methods
 */

static void ObjectPool_Lock(wObjectPool* pool)
{
	WINPR_ASSERT(pool);
	if (pool->synchronized)
		EnterCriticalSection(&pool->lock);
}

static void ObjectPool_Unlock(wObjectPool* pool)
{
	WINPR_ASSERT(pool);
	if (pool->synchronized)
		LeaveCriticalSection(&pool->lock);
}

/**
 * Gets an object from the pool.
 */

void* ObjectPool_Take(wObjectPool* pool)
{
	void* obj = NULL;

	ObjectPool_Lock(pool);

	if (pool->size > 0)
		obj = pool->array[--(pool->size)];

	if (!obj)
	{
		if (pool->object.fnObjectNew)
			obj = pool->object.fnObjectNew(NULL);
	}

	if (pool->object.fnObjectInit)
		pool->object.fnObjectInit(obj);

	ObjectPool_Unlock(pool);

	return obj;
}

/**
 * Returns an object to the pool.
 */

void ObjectPool_Return(wObjectPool* pool, void* obj)
{
	ObjectPool_Lock(pool);

	if ((pool->size + 1) >= pool->capacity)
	{
		size_t new_cap;
		void** new_arr;

		new_cap = pool->capacity * 2;
		new_arr = (void**)realloc(pool->array, sizeof(void*) * new_cap);
		if (!new_arr)
			goto out;

		pool->array = new_arr;
		pool->capacity = new_cap;
	}

	pool->array[(pool->size)++] = obj;

	if (pool->object.fnObjectUninit)
		pool->object.fnObjectUninit(obj);

out:
	ObjectPool_Unlock(pool);
}

wObject* ObjectPool_Object(wObjectPool* pool)
{
	WINPR_ASSERT(pool);
	return &pool->object;
}

/**
 * Releases the buffers currently cached in the pool.
 */

void ObjectPool_Clear(wObjectPool* pool)
{
	ObjectPool_Lock(pool);

	while (pool->size > 0)
	{
		(pool->size)--;

		if (pool->object.fnObjectFree)
			pool->object.fnObjectFree(pool->array[pool->size]);
	}

	ObjectPool_Unlock(pool);
}

/**
 * Construction, Destruction
 */

wObjectPool* ObjectPool_New(BOOL synchronized)
{
	wObjectPool* pool = NULL;

	pool = (wObjectPool*)calloc(1, sizeof(wObjectPool));

	if (pool)
	{
		pool->capacity = 32;
		pool->size = 0;
		pool->array = (void**)calloc(pool->capacity, sizeof(void*));
		if (!pool->array)
		{
			free(pool);
			return NULL;
		}
		pool->synchronized = synchronized;

		if (pool->synchronized)
			InitializeCriticalSectionAndSpinCount(&pool->lock, 4000);
	}

	return pool;
}

void ObjectPool_Free(wObjectPool* pool)
{
	if (pool)
	{
		ObjectPool_Clear(pool);

		if (pool->synchronized)
			DeleteCriticalSection(&pool->lock);

		free(pool->array);

		free(pool);
	}
}
