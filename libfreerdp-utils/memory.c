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

void *
xmalloc(size_t size)
{
	void * mem;

	if (size < 1)
	{
		size = 1;
	}
	mem = malloc(size);
	if (mem == NULL)
	{
		perror("xmalloc");
	}
	return mem;
}

void *
xrealloc(void * oldmem, size_t size)
{
	void * mem;

	if (size < 1)
	{
		size = 1;
	}
	mem = realloc(oldmem, size);
	if (mem == NULL)
	{
		perror("xrealloc");
	}
	return mem;
}

void
xfree(void * mem)
{
	if (mem != NULL)
	{
		free(mem);
	}
}

char *
xstrdup(const char * s)
{
	char * mem;

#ifdef _WIN32
	mem = _strdup(s);
#else
	mem = strdup(s);
#endif

	if (mem == NULL)
	{
		perror("strdup");
	}

	return mem;
}
