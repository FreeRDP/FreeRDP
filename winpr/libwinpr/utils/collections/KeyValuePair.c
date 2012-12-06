/**
 * WinPR: Windows Portable Runtime
 * System.Collections.Generic.KeyValuePair<TKey,TValue>
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
 * C equivalent of the C# KeyValuePair<TKey, TValue> Structure:
 * http://msdn.microsoft.com/en-us/library/5tbh8a42.aspx
 */

wKeyValuePair* KeyValuePair_New(void* key, void* value)
{
	wKeyValuePair* keyValuePair;

	keyValuePair = (wKeyValuePair*) malloc(sizeof(wKeyValuePair));

	keyValuePair->key = key;
	keyValuePair->value = value;

	return keyValuePair;
}

void KeyValuePair_Free(wKeyValuePair* keyValuePair)
{
	free(keyValuePair);
}
