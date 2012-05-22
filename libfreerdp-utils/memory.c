/*
 * FreeRDP: A Remote Desktop Protocol Client
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freerdp/utils/memory.h>

/**
 * Allocate memory.
 * This function is used to secure a malloc call.
 * It verifies its return value, and logs an error if the allocation failed.
 *
 * @param size - number of bytes to allocate. If the size is < 1, it will default to 1.
 *
 * @return a pointer to the allocated buffer. NULL if the allocation failed.
 */

void* xmalloc(size_t size)
{
	void* mem;

	if (size < 1)
		size = 1;

	mem = malloc(size);

	if (mem == NULL)
	{
		perror("xmalloc");
		printf("xmalloc: failed to allocate memory of size: %d\n", (int) size);
	}

	return mem;
}

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
 * Reallocate memory.
 * This function is used to secure a realloc call.
 * It verifies its return value, and logs an error if the allocation failed.
 *
 * @param ptr - pointer to the buffer that needs reallocation. This can be NULL, in which case a new buffer is allocated.
 * @param size - number of bytes to allocate. If the size is < 1, it will default to 1.
 *
 * @return a pointer to the reallocated buffer. NULL if the allocation failed (in which case the 'ptr' argument is untouched).
 */

void* xrealloc(void* ptr, size_t size)
{
	void* mem;

	if (size < 1)
		size = 1;

	mem = realloc(ptr, size);

	if (mem == NULL)
		perror("xrealloc");

	return mem;
}

/**
 * Free memory.
 * This function is used to secure a free call.
 * It verifies that the pointer is valid (non-NULL) before trying to deallocate it's buffer.
 *
 * @param ptr - pointer to a buffer that needs deallocation. If ptr is NULL, nothing will be done (no segfault).
 */

void xfree(void* ptr)
{
	if (ptr != NULL)
		free(ptr);
}

/**
 * Duplicate a string in memory.
 * This function is used to secure the strdup function.
 * It will allocate a new memory buffer and copy the string content in it.
 * If allocation fails, it will log an error.
 *
 * @param str - pointer to the character string to copy. If str is NULL, nothing is done.
 *
 * @return a pointer to a newly allocated character string containing the same bytes as str.
 * NULL if an allocation error occurred, or if the str parameter was NULL.
 */

char* xstrdup(const char* str)
{
	char* mem;

	if (str == NULL)
		return NULL;

#ifdef _WIN32
	mem = _strdup(str);
#else
	mem = strdup(str);
#endif

	if (mem == NULL)
		perror("strdup");

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
#elif defined(__APPLE__) && defined(__MACH__)
	mem = xmalloc(wcslen(wstr));
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
	out = xstrdup(str);
	if(out != NULL)
	{
		p = out;
		while(*p != '\0')
		{
			c = toupper((unsigned char)*p);
			*p++ = (char)c;
		}
	}
	return out;
}
