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

BOOL ArrayList_Lock(wArrayList* arrayList)
{
	return (WaitForSingleObject(arrayList->mutex, INFINITE) == WAIT_OBJECT_0) ? TRUE : FALSE;
}

/**
 * Unlock access to the ArrayList
 */

BOOL ArrayList_Unlock(wArrayList* arrayList)
{
	return ReleaseMutex(arrayList->mutex);
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

void ArrayList_Shift(wArrayList* arrayList, int index, int count)
{
	if (count > 0)
	{
		if (arrayList->size + count > arrayList->capacity)
		{
			arrayList->capacity *= arrayList->growthFactor;
			arrayList->array = (void**) realloc(arrayList->array, sizeof(void*) * arrayList->capacity);
		}

		MoveMemory(&arrayList->array[index + count], &arrayList->array[index], (arrayList->size - index) * sizeof(void*));
		arrayList->size += count;
	}
	else if (count < 0)
	{
		MoveMemory(&arrayList->array[index + count], &arrayList->array[index], (arrayList->size - index) * sizeof(void*));
		arrayList->size += count;
	}
}

/**
 * Removes all elements from the ArrayList.
 */

void ArrayList_Clear(wArrayList* arrayList)
{
	if (arrayList->synchronized)
		WaitForSingleObject(arrayList->mutex, INFINITE);

	if (arrayList->synchronized)
		ReleaseMutex(arrayList->mutex);
}

/**
 * Determines whether an element is in the ArrayList.
 */

BOOL ArrayList_Contains(wArrayList* arrayList, void* obj)
{
	if (arrayList->synchronized)
		WaitForSingleObject(arrayList->mutex, INFINITE);

	if (arrayList->synchronized)
		ReleaseMutex(arrayList->mutex);

	return FALSE;
}

/**
 * Adds an object to the end of the ArrayList.
 */

int ArrayList_Add(wArrayList* arrayList, void* obj)
{
	int index;

	if (arrayList->synchronized)
		WaitForSingleObject(arrayList->mutex, INFINITE);

	arrayList->array[arrayList->size++] = obj;
	index = arrayList->size;

	if (arrayList->synchronized)
		ReleaseMutex(arrayList->mutex);

	return index;
}

/*
 * Inserts an element into the ArrayList at the specified index.
 */

void ArrayList_Insert(wArrayList* arrayList, int index, void* obj)
{
	if (arrayList->synchronized)
		WaitForSingleObject(arrayList->mutex, INFINITE);

	if ((index >= 0) && (index < arrayList->size))
	{
		ArrayList_Shift(arrayList, index, 1);
		arrayList->array[index] = obj;
	}

	if (arrayList->synchronized)
		ReleaseMutex(arrayList->mutex);
}

/**
 * Removes the first occurrence of a specific object from the ArrayList.
 */

void ArrayList_Remove(wArrayList* arrayList, void* obj)
{
	int index;
	BOOL found = FALSE;

	if (arrayList->synchronized)
		WaitForSingleObject(arrayList->mutex, INFINITE);

	for (index = 0; index < arrayList->size; index++)
	{
		if (arrayList->array[index] == obj)
		{
			found = TRUE;
			break;
		}
	}

	if (found)
		ArrayList_Shift(arrayList, index, -1);

	if (arrayList->synchronized)
		ReleaseMutex(arrayList->mutex);
}

/**
 * Removes the element at the specified index of the ArrayList.
 */

void ArrayList_RemoveAt(wArrayList* arrayList, int index)
{
	if (arrayList->synchronized)
		WaitForSingleObject(arrayList->mutex, INFINITE);

	if ((index >= 0) && (index < arrayList->size))
	{
		ArrayList_Shift(arrayList, index, -1);
	}

	if (arrayList->synchronized)
		ReleaseMutex(arrayList->mutex);
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
		WaitForSingleObject(arrayList->mutex, INFINITE);

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
		ReleaseMutex(arrayList->mutex);

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
		WaitForSingleObject(arrayList->mutex, INFINITE);

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
		ReleaseMutex(arrayList->mutex);

	return index;
}

/**
 * Construction, Destruction
 */

wArrayList* ArrayList_New(BOOL synchronized)
{
	wArrayList* arrayList = NULL;

	arrayList = (wArrayList*) malloc(sizeof(wArrayList));

	if (arrayList)
	{
		arrayList->synchronized = synchronized;

		arrayList->size = 0;
		arrayList->capacity = 32;
		arrayList->growthFactor = 2;

		arrayList->array = (void**) malloc(sizeof(void*) * arrayList->capacity);

		arrayList->mutex = CreateMutex(NULL, FALSE, NULL);
	}

	return arrayList;
}

void ArrayList_Free(wArrayList* arrayList)
{
	CloseHandle(arrayList->mutex);
	free(arrayList);
}
