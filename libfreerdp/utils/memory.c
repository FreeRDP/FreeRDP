/*
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Memory Utils
 *
 * Copyright 2009-2011 Jay Sorg
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#include <freerdp/utils/memory.h>

/**
 * Allocate memory initialized to zero.
 * This function is used to secure a calloc call.
 * It verifies its return value, and logs an error if the allocation failed.
 *
 * @param size - number of bytes to allocate. If the size is < 1, it will default to 1.
 *
 * @return a pointer to the allocated and zeroed buffer. NULL if the allocation failed.
 */
void* xzalloc(size_t size)
{
	void* mem;

	if (size < 1)
		size = 1;

	mem = calloc(1, size);

	if (mem == NULL)
	{
		perror("xzalloc");
		printf("xzalloc: failed to allocate memory of size: %d\n", (int) size);
	}

	return mem;
}

/**
 * Duplicate a wide string in memory.
 * This function is used to secure a call to wcsdup.
 * It verifies the return value, and logs a message if an allocation error occurred.
 *
 * @param wstr - pointer to the wide-character string to duplicate. If wstr is NULL, nothing will be done.
 *
 * @return a pointer to the newly allocated string, containing the same data as wstr.
 * NULL if an allocation error occurred (or if wstr was NULL).
 */

wchar_t* xwcsdup(const wchar_t* wstr)
{
	wchar_t* mem;

	if (wstr == NULL)
		return NULL;

#ifdef _WIN32
	mem = _wcsdup(wstr);
#elif sun
	mem = wsdup(wstr);
#elif (defined(__APPLE__) && defined(__MACH__)) || defined(ANDROID)
	mem = malloc(wcslen(wstr) * sizeof(wchar_t));
	if (mem != NULL)
		wcscpy(mem, wstr);
#else
	mem = wcsdup(wstr);
#endif

	if (mem == NULL)
		perror("wstrdup");

	return mem;
}

/**
 * Create an uppercase version of the given string.
 * This function will duplicate the string (using xstrdup()) and change its content to all uppercase.
 * The original string is untouched.
 *
 * @param str - pointer to the character string to convert. This content is untouched by the function.
 *
 * @return pointer to a newly allocated character string, containing the same content as str, converted to uppercase.
 * NULL if an allocation error occured.
 */
char* xstrtoup(const char* str)
{
	char* out;
	char* p;
	int c;

	out = _strdup(str);

	if (out != NULL)
	{
		p = out;

		while (*p != '\0')
		{
			c = toupper((unsigned char)*p);
			*p++ = (char) c;
		}
	}

	return out;
}
