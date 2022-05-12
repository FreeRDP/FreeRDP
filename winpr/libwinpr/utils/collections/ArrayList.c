/**
 * WinPR: Windows Portable Runtime
 * System.Collections.ArrayList
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

#include <stdarg.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/collections.h>

#if defined(_WIN32) && (_MSC_VER < 1800) && !defined(__MINGW32__)
#define va_copy(dest, src) (dest = src)
#endif

struct s_wArrayList
{
	size_t capacity;
	size_t growthFactor;
	BOOL synchronized;

	size_t size;
	void** array;
	CRITICAL_SECTION lock;

	wObject object;
};

/**
 * C equivalent of the C# ArrayList Class:
 * http://msdn.microsoft.com/en-us/library/system.collections.arraylist.aspx
 */

/**
 * Properties
 */

/**
 * Gets or sets the number of elements that the ArrayList can contain.
 */

size_t ArrayList_Capacity(wArrayList* arrayList)
{
	WINPR_ASSERT(arrayList);
	return arrayList->capacity;
}

/**
 * Gets the number of elements actually contained in the ArrayList.
 */

size_t ArrayList_Count(wArrayList* arrayList)
{
	WINPR_ASSERT(arrayList);
	return arrayList->size;
}

/**
 * Gets the internal list of items contained in the ArrayList.
 */

size_t ArrayList_Items(wArrayList* arrayList, ULONG_PTR** ppItems)
{
	WINPR_ASSERT(arrayList);
	*ppItems = (ULONG_PTR*)arrayList->array;
	return arrayList->size;
}

/**
 * Gets a value indicating whether the ArrayList has a fixed size.
 */

BOOL ArrayList_IsFixedSized(wArrayList* arrayList)
{
	WINPR_ASSERT(arrayList);
	return FALSE;
}

/**
 * Gets a value indicating whether the ArrayList is read-only.
 */

BOOL ArrayList_IsReadOnly(wArrayList* arrayList)
{
	WINPR_ASSERT(arrayList);
	return FALSE;
}

/**
 * Gets a value indicating whether access to the ArrayList is synchronized (thread safe).
 */

BOOL ArrayList_IsSynchronized(wArrayList* arrayList)
{
	WINPR_ASSERT(arrayList);
	return arrayList->synchronized;
}

/**
 * Lock access to the ArrayList
 */

static void ArrayList_Lock_Conditional(wArrayList* arrayList)
{
	WINPR_ASSERT(arrayList);
	if (arrayList->synchronized)
		EnterCriticalSection(&arrayList->lock);
}

void ArrayList_Lock(wArrayList* arrayList)
{
	WINPR_ASSERT(arrayList);
	EnterCriticalSection(&arrayList->lock);
}

/**
 * Unlock access to the ArrayList
 */

static void ArrayList_Unlock_Conditional(wArrayList* arrayList)
{
	WINPR_ASSERT(arrayList);
	if (arrayList->synchronized)
		LeaveCriticalSection(&arrayList->lock);
}

void ArrayList_Unlock(wArrayList* arrayList)
{
	WINPR_ASSERT(arrayList);
	LeaveCriticalSection(&arrayList->lock);
}

/**
 * Gets the element at the specified index.
 */

void* ArrayList_GetItem(wArrayList* arrayList, size_t index)
{
	void* obj = NULL;

	WINPR_ASSERT(arrayList);
	if (index < arrayList->size)
	{
		obj = arrayList->array[index];
	}

	return obj;
}

/**
 * Sets the element at the specified index.
 */

BOOL ArrayList_SetItem(wArrayList* arrayList, size_t index, const void* obj)
{
	WINPR_ASSERT(arrayList);
	if (index >= arrayList->size)
		return FALSE;

	if (arrayList->object.fnObjectNew)
	{
		arrayList->array[index] = arrayList->object.fnObjectNew(obj);
		if (obj && !arrayList->array[index])
			return FALSE;
	}
	else
	{
		union
		{
			const void* cpv;
			void* pv;
		} cnv;
		cnv.cpv = obj;
		arrayList->array[index] = cnv.pv;
	}
	return TRUE;
}

/**
 * Methods
 */
static BOOL ArrayList_EnsureCapacity(wArrayList* arrayList, size_t count)
{
	WINPR_ASSERT(arrayList);
	WINPR_ASSERT(count > 0);

	if (arrayList->size + count > arrayList->capacity)
	{
		void** newArray;
		size_t newCapacity = arrayList->capacity * arrayList->growthFactor;
		if (newCapacity < arrayList->size + count)
			newCapacity = arrayList->size + count;

		newArray = (void**)realloc(arrayList->array, sizeof(void*) * newCapacity);

		if (!newArray)
			return FALSE;

		arrayList->array = newArray;
		arrayList->capacity = newCapacity;
	}

	return TRUE;
}
/**
 * Shift a section of the list.
 */

static BOOL ArrayList_Shift(wArrayList* arrayList, size_t index, SSIZE_T count)
{
	WINPR_ASSERT(arrayList);
	if (count > 0)
	{
		if (!ArrayList_EnsureCapacity(arrayList, count))
			return FALSE;

		MoveMemory(&arrayList->array[index + count], &arrayList->array[index],
		           (arrayList->size - index) * sizeof(void*));
		arrayList->size += count;
	}
	else if (count < 0)
	{
		INT64 chunk = arrayList->size - index + count;

		if (chunk > 0)
			MoveMemory(&arrayList->array[index], &arrayList->array[index - count],
			           chunk * sizeof(void*));

		arrayList->size += count;
	}

	return TRUE;
}

/**
 * Removes all elements from the ArrayList.
 */

void ArrayList_Clear(wArrayList* arrayList)
{
	size_t index;

	WINPR_ASSERT(arrayList);
	ArrayList_Lock_Conditional(arrayList);

	for (index = 0; index < arrayList->size; index++)
	{
		if (arrayList->object.fnObjectFree)
			arrayList->object.fnObjectFree(arrayList->array[index]);

		arrayList->array[index] = NULL;
	}

	arrayList->size = 0;

	ArrayList_Unlock_Conditional(arrayList);
}

/**
 * Determines whether an element is in the ArrayList.
 */

BOOL ArrayList_Contains(wArrayList* arrayList, const void* obj)
{
	size_t index;
	BOOL rc = FALSE;

	WINPR_ASSERT(arrayList);
	ArrayList_Lock_Conditional(arrayList);

	for (index = 0; index < arrayList->size; index++)
	{
		rc = arrayList->object.fnObjectEquals(arrayList->array[index], obj);

		if (rc)
			break;
	}

	ArrayList_Unlock_Conditional(arrayList);

	return rc;
}

#if defined(WITH_WINPR_DEPRECATED)
int ArrayList_Add(wArrayList* arrayList, const void* obj)
{
	WINPR_ASSERT(arrayList);
	if (!ArrayList_Append(arrayList, obj))
		return -1;
	return (int)ArrayList_Count(arrayList) - 1;
}
#endif

/**
 * Adds an object to the end of the ArrayList.
 */

BOOL ArrayList_Append(wArrayList* arrayList, const void* obj)
{
	size_t index;
	BOOL rc = FALSE;

	WINPR_ASSERT(arrayList);
	ArrayList_Lock_Conditional(arrayList);

	if (!ArrayList_EnsureCapacity(arrayList, 1))
		goto out;

	index = arrayList->size++;
	rc = ArrayList_SetItem(arrayList, index, obj);
out:

	ArrayList_Unlock_Conditional(arrayList);

	return rc;
}

/*
 * Inserts an element into the ArrayList at the specified index.
 */

BOOL ArrayList_Insert(wArrayList* arrayList, size_t index, const void* obj)
{
	BOOL ret = TRUE;

	WINPR_ASSERT(arrayList);
	ArrayList_Lock_Conditional(arrayList);

	if (index < arrayList->size)
	{
		if (!ArrayList_Shift(arrayList, index, 1))
		{
			ret = FALSE;
		}
		else
		{
			ArrayList_SetItem(arrayList, index, obj);
		}
	}

	ArrayList_Unlock_Conditional(arrayList);

	return ret;
}

/**
 * Removes the first occurrence of a specific object from the ArrayList.
 */

BOOL ArrayList_Remove(wArrayList* arrayList, const void* obj)
{
	size_t index;
	BOOL found = FALSE;
	BOOL ret = TRUE;

	WINPR_ASSERT(arrayList);
	ArrayList_Lock_Conditional(arrayList);

	for (index = 0; index < arrayList->size; index++)
	{
		if (arrayList->object.fnObjectEquals(arrayList->array[index], obj))
		{
			found = TRUE;
			break;
		}
	}

	if (found)
	{
		if (arrayList->object.fnObjectFree)
			arrayList->object.fnObjectFree(arrayList->array[index]);

		ret = ArrayList_Shift(arrayList, index, -1);
	}

	ArrayList_Unlock_Conditional(arrayList);

	return ret;
}

/**
 * Removes the element at the specified index of the ArrayList.
 */

BOOL ArrayList_RemoveAt(wArrayList* arrayList, size_t index)
{
	BOOL ret = TRUE;

	WINPR_ASSERT(arrayList);
	ArrayList_Lock_Conditional(arrayList);

	if (index < arrayList->size)
	{
		if (arrayList->object.fnObjectFree)
			arrayList->object.fnObjectFree(arrayList->array[index]);

		ret = ArrayList_Shift(arrayList, index, -1);
	}

	ArrayList_Unlock_Conditional(arrayList);

	return ret;
}

/**
 * Searches for the specified Object and returns the zero-based index of the first occurrence within
 * the entire ArrayList.
 *
 * Searches for the specified Object and returns the zero-based index of the last occurrence within
 * the range of elements in the ArrayList that extends from the first element to the specified
 * index.
 *
 * Searches for the specified Object and returns the zero-based index of the last occurrence within
 * the range of elements in the ArrayList that contains the specified number of elements and ends at
 * the specified index.
 */

SSIZE_T ArrayList_IndexOf(wArrayList* arrayList, const void* obj, SSIZE_T startIndex, SSIZE_T count)
{
	SSIZE_T index, sindex, cindex;
	BOOL found = FALSE;

	WINPR_ASSERT(arrayList);
	ArrayList_Lock_Conditional(arrayList);

	sindex = (size_t)startIndex;
	if (startIndex < 0)
		sindex = 0;

	cindex = (size_t)count;
	if (count < 0)
		cindex = arrayList->size;

	for (index = sindex; index < sindex + cindex; index++)
	{
		if (arrayList->object.fnObjectEquals(arrayList->array[index], obj))
		{
			found = TRUE;
			break;
		}
	}

	if (!found)
		index = -1;

	ArrayList_Unlock_Conditional(arrayList);

	return index;
}

/**
 * Searches for the specified Object and returns the zero-based index of the last occurrence within
 * the entire ArrayList.
 *
 * Searches for the specified Object and returns the zero-based index of the last occurrence within
 * the range of elements in the ArrayList that extends from the first element to the specified
 * index.
 *
 * Searches for the specified Object and returns the zero-based index of the last occurrence within
 * the range of elements in the ArrayList that contains the specified number of elements and ends at
 * the specified index.
 */

SSIZE_T ArrayList_LastIndexOf(wArrayList* arrayList, const void* obj, SSIZE_T startIndex,
                              SSIZE_T count)
{
	SSIZE_T index, sindex, cindex;
	BOOL found = FALSE;

	WINPR_ASSERT(arrayList);
	ArrayList_Lock_Conditional(arrayList);

	sindex = (size_t)startIndex;
	if (startIndex < 0)
		sindex = 0;

	cindex = (size_t)count;
	if (count < 0)
		cindex = arrayList->size;

	for (index = sindex + cindex; index > sindex; index--)
	{
		if (arrayList->object.fnObjectEquals(arrayList->array[index - 1], obj))
		{
			found = TRUE;
			break;
		}
	}

	if (!found)
		index = -1;

	ArrayList_Unlock_Conditional(arrayList);

	return index;
}

static BOOL ArrayList_DefaultCompare(const void* objA, const void* objB)
{
	return objA == objB ? TRUE : FALSE;
}

wObject* ArrayList_Object(wArrayList* arrayList)
{
	WINPR_ASSERT(arrayList);
	return &arrayList->object;
}

BOOL ArrayList_ForEach(wArrayList* arrayList, ArrayList_ForEachFkt fkt, ...)
{
	BOOL rc;
	va_list ap;
	va_start(ap, fkt);
	rc = ArrayList_ForEachAP(arrayList, fkt, ap);
	va_end(ap);

	return rc;
}

BOOL ArrayList_ForEachAP(wArrayList* arrayList, ArrayList_ForEachFkt fkt, va_list ap)
{
	size_t index, count;
	BOOL rc = FALSE;
	va_list cap;

	WINPR_ASSERT(arrayList);
	WINPR_ASSERT(fkt);

	ArrayList_Lock_Conditional(arrayList);
	count = ArrayList_Count(arrayList);
	for (index = 0; index < count; index++)
	{
		BOOL rs;
		void* obj = ArrayList_GetItem(arrayList, index);
		va_copy(cap, ap);
		rs = fkt(obj, index, cap);
		va_end(cap);
		if (!rs)
			goto fail;
	}
	rc = TRUE;
fail:
	ArrayList_Unlock_Conditional(arrayList);
	return rc;
}

/**
 * Construction, Destruction
 */

wArrayList* ArrayList_New(BOOL synchronized)
{
	wObject* obj;
	wArrayList* arrayList = NULL;
	arrayList = (wArrayList*)calloc(1, sizeof(wArrayList));

	if (!arrayList)
		return NULL;

	arrayList->synchronized = synchronized;
	arrayList->growthFactor = 2;
	obj = ArrayList_Object(arrayList);
	if (!obj)
		goto fail;
	obj->fnObjectEquals = ArrayList_DefaultCompare;
	if (!ArrayList_EnsureCapacity(arrayList, 32))
		goto fail;

	InitializeCriticalSectionAndSpinCount(&arrayList->lock, 4000);
	return arrayList;
fail:
	ArrayList_Free(arrayList);
	return NULL;
}

void ArrayList_Free(wArrayList* arrayList)
{
	if (!arrayList)
		return;

	ArrayList_Clear(arrayList);
	DeleteCriticalSection(&arrayList->lock);
	free(arrayList->array);
	free(arrayList);
}
