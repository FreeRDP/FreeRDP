/**
 * WinPR: Windows Portable Runtime
 * System.Collections.Specialized.ListDictionary
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
 * C equivalent of the C# ListDictionary Class:
 * http://msdn.microsoft.com/en-us/library/system.collections.specialized.listdictionary.aspx
 */

/**
 * Properties
 */

/**
 * Gets the number of key/value pairs contained in the ListDictionary.
 */

int ListDictionary_Count(wListDictionary* listDictionary)
{
	return 0;
}

/**
 * Gets a value indicating whether the ListDictionary has a fixed size.
 */

BOOL ListDictionary_IsFixedSized(wListDictionary* listDictionary)
{
	return FALSE;
}

/**
 * Gets a value indicating whether the ListDictionary is read-only.
 */

BOOL ListDictionary_IsReadOnly(wListDictionary* listDictionary)
{
	return FALSE;
}

/**
 * Gets a value indicating whether the ListDictionary is synchronized (thread safe).
 */

BOOL ListDictionary_IsSynchronized(wListDictionary* listDictionary)
{
	return listDictionary->synchronized;
}

/**
 * Methods
 */

/**
 * Adds an entry with the specified key and value into the ListDictionary.
 */

void ListDictionary_Add(wListDictionary* listDictionary, void* key, void* value)
{

}

/**
 * Removes all entries from the ListDictionary.
 */

void ListDictionary_Clear(wListDictionary* listDictionary)
{

}

/**
 * Determines whether the ListDictionary contains a specific key.
 */

BOOL ListDictionary_Contains(wListDictionary* listDictionary, void* key)
{
	return FALSE;
}

/**
 * Removes the entry with the specified key from the ListDictionary.
 */

void ListDictionary_Remove(wListDictionary* listDictionary, void* key)
{

}

/**
 * Construction, Destruction
 */

wListDictionary* ListDictionary_New(BOOL synchronized)
{
	wListDictionary* listDictionary = NULL;

	listDictionary = (wListDictionary*) malloc(sizeof(wListDictionary));

	if (listDictionary)
	{
		listDictionary->synchronized = synchronized;
	}

	return listDictionary;
}

void ListDictionary_Free(wListDictionary* listDictionary)
{
	free(listDictionary);
}

