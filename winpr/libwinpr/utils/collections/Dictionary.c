/**
 * WinPR: Windows Portable Runtime
 * System.Collections.DictionaryBase
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
 * C equivalent of the C# DictionaryBase Class:
 * http://msdn.microsoft.com/en-us/library/system.collections.dictionarybase.aspx
 */

/**
 * Properties
 */

/**
 * Gets the number of key/value pairs contained in the Dictionary.
 */

int Dictionary_Count(wDictionary* dictionary)
{
	return 0;
}

/**
 * Gets a value indicating whether the Dictionary has a fixed size.
 */

BOOL Dictionary_IsFixedSized(wDictionary* dictionary)
{
	return FALSE;
}

/**
 * Gets a value indicating whether the Dictionary is read-only.
 */

BOOL Dictionary_IsReadOnly(wDictionary* dictionary)
{
	return FALSE;
}

/**
 * Gets a value indicating whether the Dictionary is synchronized (thread safe).
 */

BOOL Dictionary_IsSynchronized(wDictionary* dictionary)
{
	return dictionary->synchronized;
}

/**
 * Methods
 */

/**
 * Adds an entry with the specified key and value into the Dictionary.
 */

void Dictionary_Add(wDictionary* dictionary, void* key, void* value)
{

}

/**
 * Removes all entries from the Dictionary.
 */

void Dictionary_Clear(wDictionary* dictionary)
{

}

/**
 * Determines whether the Dictionary contains a specific key.
 */

BOOL Dictionary_Contains(wDictionary* dictionary, void* key)
{
	return FALSE;
}

/**
 * Removes the entry with the specified key from the Dictionary.
 */

void Dictionary_Remove(wDictionary* dictionary, void* key)
{

}

/**
 * Construction, Destruction
 */

wDictionary* Dictionary_New(BOOL synchronized)
{
	wDictionary* dictionary = NULL;

	dictionary = (wDictionary*) malloc(sizeof(wDictionary));

	if (dictionary)
	{
		dictionary->synchronized = synchronized;
	}

	return dictionary;
}

void Dictionary_Free(wDictionary* dictionary)
{
	free(dictionary);
}

