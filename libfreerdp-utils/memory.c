/**
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
 * @param size
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
 * @param size
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
 * @param ptr
 * @param size
 */

void* xrealloc(void* ptr, size_t size)
{
	void* mem;

	if (size < 1)
		size = 1;

	if (ptr == NULL)
	{
		printf("xrealloc: null pointer given\n");
		return NULL;
	}

	mem = realloc(ptr, size);

	if (mem == NULL)
		perror("xrealloc");

	return mem;
}

/**
 * Free memory.
 * @param mem
 */

void xfree(void* ptr)
{
	if (ptr != NULL)
		free(ptr);
}

/**
 * Duplicate a string in memory.
 * @param str
 * @return
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
