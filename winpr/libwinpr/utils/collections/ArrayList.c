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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#include <winpr/collections.h>

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

int ArrayList_Capacity(wArrayList* arrayList)
{
	return arrayList->capacity;
}

/**
 * Gets the number of elements actually contained in the ArrayList.
 */

int ArrayList_Count(wArrayList* arrayList)
{
	return arrayList->size;
}

/**
 * Gets a value indicating whether the ArrayList has a fixed size.
 */

BOOL ArrayList_IsFixedSized(wArrayList* arrayList)
{
	return FALSE;
}

/**
 * Gets a value indicating whether the ArrayList is read-only.
 */

BOOL ArrayList_IsReadOnly(wArrayList* arrayList)
{
	return FALSE;
}

/**
 * Gets a value indicating whether access to the ArrayList is synchronized (thread safe).
 */

BOOL ArrayList_IsSynchronized(wArrayList* arrayList)
{
	return arrayList->synchronized;
}

/**
 * Lock access to the ArrayList
 */

void ArrayList_Lock(wArrayList* arrayList)
{
	EnterCriticalSection(&arrayList->lock);
}

/**
 * Unlock access to the ArrayList
 */

void ArrayList_Unlock(wArrayList* arrayList)
{
	LeaveCriticalSection(&arrayList->lock);
}

/**
 * Gets the element at the specified index.
 */

void* ArrayList_GetItem(wArrayList* arrayList, int index)
{
	void* obj = NULL;

	if ((index >= 0) && (index < arrayList->size))
	{
		obj = arrayList->array[index];
	}

	return obj;
}

/**
 * Sets the element at the specified index.
 */

void ArrayList_SetItem(wArrayList* arrayList, int index, void* obj)
{
	if ((index >= 0) && (index < arrayList->size))
	{
		arrayList->array[index] = obj;
	}
}

/**
 * Methods
 */

/**
 * Shift a section of the list.
 */

BOOL ArrayList_Shift(wArrayList* arrayList, int index, int count)
{
	if (count > 0)
	{
		if (arrayList->size + count > arrayList->capacity)
		{
			void **newArray;
			int newCapacity = arrayList->capacity * arrayList->growthFactor;

			newArray = (void **)realloc(arrayList->array, sizeof(void*) * newCapacity);
			if (!newArray)
				return FALSE;
			arrayList->array = newArray;
			arrayList->capacity = newCapacity;
		}

		MoveMemory(&arrayList->array[index + count], &arrayList->array[index], (arrayList->size - index) * sizeof(void*));
		arrayList->size += count;
	}
	else if (count < 0)
	{
		int chunk = arrayList->size - index + count;
		if (chunk > 0)
			MoveMemory(&arrayList->array[index], &arrayList->array[index - count], chunk * sizeof(void*));
		arrayList->size += count;
	}
	return TRUE;
}

/**
 * Removes all elements from the ArrayList.
 */

void ArrayList_Clear(wArrayList* arrayList)
{
	int index;

	if (arrayList->synchronized)
		EnterCriticalSection(&arrayList->lock);

	for (index = 0; index < arrayList->size; index++)
	{
		if (arrayList->object.fnObjectFree)
			arrayList->object.fnObjectFree(arrayList->array[index]);

		arrayList->array[index] = NULL;
	}

	arrayList->size = 0;

	if (arrayList->synchronized)
		LeaveCriticalSection(&arrayList->lock);
}

/**
 * Determines whether an element is in the ArrayList.
 */

BOOL ArrayList_Contains(wArrayList* arrayList, void* obj)
{
	if (arrayList->synchronized)
		EnterCriticalSection(&arrayList->lock);

	if (arrayList->synchronized)
		LeaveCriticalSection(&arrayList->lock);

	return FALSE;
}

/**
 * Adds an object to the end of the ArrayList.
 */

int ArrayList_Add(wArrayList* arrayList, void* obj)
{
	int index = -1;

	if (arrayList->synchronized)
		EnterCriticalSection(&arrayList->lock);

	if (arrayList->size + 1 > arrayList->capacity)
	{
		void **newArray;
		int newCapacity = arrayList->capacity * arrayList->growthFactor;
		newArray = (void **)realloc(arrayList->array, sizeof(void*) * newCapacity);
		if (!newArray)
			goto out;

		arrayList->array = newArray;
		arrayList->capacity = newCapacity;
	}

	arrayList->array[arrayList->size++] = obj;
	index = arrayList->size;

out:
	if (arrayList->synchronized)
		LeaveCriticalSection(&arrayList->lock);

	return index;
}

/*
 * Inserts an element into the ArrayList at the specified index.
 */

BOOL ArrayList_Insert(wArrayList* arrayList, int index, void* obj)
{
	BOOL ret = TRUE;
	if (arrayList->synchronized)
		EnterCriticalSection(&arrayList->lock);

	if ((index >= 0) && (index < arrayList->size))
	{
		if (!ArrayList_Shift(arrayList, index, 1))
		{
			ret = FALSE;
		}
		else
		{
			arrayList->array[index] = obj;
		}
	}

	if (arrayList->synchronized)
		LeaveCriticalSection(&arrayList->lock);
	return ret;
}

/**
 * Removes the first occurrence of a specific object from the ArrayList.
 */

BOOL ArrayList_Remove(wArrayList* arrayList, void* obj)
{
	int index;
	BOOL found = FALSE;
	BOOL ret = TRUE;

	if (arrayList->synchronized)
		EnterCriticalSection(&arrayList->lock);

	for (index = 0; index < arrayList->size; index++)
	{
		if (arrayList->array[index] == obj)
		{
			found = TRUE;
			break;
		}
	}

	if (found)
		ret = ArrayList_Shift(arrayList, index, -1);

	if (arrayList->synchronized)
		LeaveCriticalSection(&arrayList->lock);
	return ret;
}

/**
 * Removes the element at the specified index of the ArrayList.
 */

BOOL ArrayList_RemoveAt(wArrayList* arrayList, int index)
{
	BOOL ret = TRUE;

	if (arrayList->synchronized)
		EnterCriticalSection(&arrayList->lock);

	if ((index >= 0) && (index < arrayList->size))
	{
		ret = ArrayList_Shift(arrayList, index, -1);
	}

	if (arrayList->synchronized)
		LeaveCriticalSection(&arrayList->lock);
	return ret;
}

/**
 * Searches for the specified Object and returns the zero-based index of the first occurrence within the entire ArrayList.
 *
 * Searches for the specified Object and returns the zero-based index of the last occurrence within the range of elements
 * in the ArrayList that extends from the first element to the specified index.
 *
 * Searches for the specified Object and returns the zero-based index of the last occurrence within the range of elements
 * in the ArrayList that contains the specified number of elements and ends at the specified index.
 */

int ArrayList_IndexOf(wArrayList* arrayList, void* obj, int startIndex, int count)
{
	int index;
	BOOL found = FALSE;

	if (arrayList->synchronized)
		EnterCriticalSection(&arrayList->lock);

	if (startIndex < 0)
		startIndex = 0;

	if (count < 0)
		count = arrayList->size;

	for (index = startIndex; index < startIndex + count; index++)
	{
		if (arrayList->array[index] == obj)
		{
			found = TRUE;
			break;
		}
	}

	if (!found)
		index = -1;

	if (arrayList->synchronized)
		LeaveCriticalSection(&arrayList->lock);

	return index;
}

/**
 * Searches for the specified Object and returns the zero-based index of the last occurrence within the entire ArrayList.
 *
 * Searches for the specified Object and returns the zero-based index of the last occurrence within the range of elements
 * in the ArrayList that extends from the first element to the specified index.
 *
 * Searches for the specified Object and returns the zero-based index of the last occurrence within the range of elements
 * in the ArrayList that contains the specified number of elements and ends at the specified index.
 */

int ArrayList_LastIndexOf(wArrayList* arrayList, void* obj, int startIndex, int count)
{
	int index;
	BOOL found = FALSE;

	if (arrayList->synchronized)
		EnterCriticalSection(&arrayList->lock);

	if (startIndex < 0)
		startIndex = 0;

	if (count < 0)
		count = arrayList->size;

	for (index = startIndex + count - 1; index >= startIndex; index--)
	{
		if (arrayList->array[index] == obj)
		{
			found = TRUE;
			break;
		}
	}

	if (!found)
		index = -1;

	if (arrayList->synchronized)
		LeaveCriticalSection(&arrayList->lock);

	return index;
}

/**
 * Construction, Destruction
 */

wArrayList* ArrayList_New(BOOL synchronized)
{
	wArrayList* arrayList = NULL;

	arrayList = (wArrayList *)calloc(1, sizeof(wArrayList));
	if (!arrayList)
		return NULL;

	arrayList->synchronized = synchronized;
	arrayList->capacity = 32;
	arrayList->growthFactor = 2;

	arrayList->array = (void **)malloc(arrayList->capacity * sizeof(void *));
	if (!arrayList->array)
		goto out_free;

	InitializeCriticalSectionAndSpinCount(&arrayList->lock, 4000);
	return arrayList;

out_free:
	free(arrayList);
	return NULL;
}

void ArrayList_Free(wArrayList* arrayList)
{
	ArrayList_Clear(arrayList);

	DeleteCriticalSection(&arrayList->lock);
	free(arrayList->array);
	free(arrayList);
}
