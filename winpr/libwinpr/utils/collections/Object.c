/**
 * WinPR: Windows Portable Runtime
 * Collections
 *
 * Copyright 2024 Armin Novak <anovak@thincast.com>
 * Copyright 2024 Thincast Technologies GmbH
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
#include <winpr/string.h>
#include <winpr/collections.h>

void* winpr_ObjectStringClone(const void* pvstr)
{
	const char* str = pvstr;
	if (!str)
		return NULL;
	return _strdup(str);
}

void* winpr_ObjectWStringClone(const void* pvstr)
{
	const WCHAR* str = pvstr;
	if (!str)
		return NULL;
	return _wcsdup(str);
}

void winpr_ObjectStringFree(void* pvstr)
{
	free(pvstr);
}
