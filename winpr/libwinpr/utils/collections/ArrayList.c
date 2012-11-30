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
	if (arrayList->bSynchronized)
	{

	}

	return 0;
}

/**
 * Gets the number of elements actually contained in the ArrayList.
 */

int ArrayList_Count(wArrayList* arrayList)
{
	if (arrayList->bSynchronized)
	{

	}

	return 0;
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
	return arrayList->bSynchronized;
}

/**
 * Gets or sets the element at the specified index.
 */

void* ArrayList_Item(wArrayList* arrayList, int index, void* obj)
{
	return NULL;
}

/**
 * Methods
 */

/**
 * Removes all elements from the ArrayList.
 */

void ArrayList_Clear(wArrayList* arrayList)
{
	if (arrayList->bSynchronized)
	{

	}
}

/**
 * Determines whether an element is in the ArrayList.
 */

BOOL ArrayList_Contains(wArrayList* arrayList, void* obj)
{
	if (arrayList->bSynchronized)
	{

	}

	return FALSE;
}

/**
 * Adds an object to the end of the ArrayList.
 */

int ArrayList_Add(wArrayList* arrayList, void* obj)
{
	if (arrayList->bSynchronized)
	{

	}

	return FALSE;
}

/*
 * Inserts an element into the ArrayList at the specified index.
 */

void ArrayList_Insert(wArrayList* arrayList, int index, void* obj)
{
	if (arrayList->bSynchronized)
	{

	}
}

/**
 * Removes the first occurrence of a specific object from the ArrayList.
 */

void ArrayList_Remove(wArrayList* arrayList, void* obj)
{
	if (arrayList->bSynchronized)
	{

	}
}

/**
 * Removes the element at the specified index of the ArrayList.
 */

void ArrayList_RemoveAt(wArrayList* arrayList, int index, void* obj)
{
	if (arrayList->bSynchronized)
	{

	}
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
	if (arrayList->bSynchronized)
	{

	}

	return 0;
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
	if (arrayList->bSynchronized)
	{

	}

	return 0;
}

/**
 * Construction, Destruction
 */

wArrayList* ArrayList_New(BOOL bSynchronized)
{
	wArrayList* arrayList = NULL;

	arrayList = (wArrayList*) malloc(sizeof(wArrayList));

	if (arrayList)
	{
		arrayList->bSynchronized = bSynchronized;
	}

	return arrayList;
}

void ArrayList_Free(wArrayList* arrayList)
{
	free(arrayList);
}
