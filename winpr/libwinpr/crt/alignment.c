/**
 * WinPR: Windows Portable Runtime
 * Data Alignment
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

/* Data Alignment: http://msdn.microsoft.com/en-us/library/fs9stz4e/ */

#ifndef _WIN32

#include <stdlib.h>
#include <malloc.h>

void* _aligned_malloc(size_t size, size_t alignment)
{
	void* memptr;

	if (posix_memalign(&memptr, alignment, size) != 0)
		return NULL;

	return memptr;
}

void* _aligned_realloc(void* memblock, size_t size, size_t alignment)
{
	void* memptr = NULL;

	memptr = realloc(memblock, size);

	return memptr;
}

void* _aligned_recalloc(void* memblock, size_t num, size_t size, size_t alignment)
{
	return NULL;
}

void* _aligned_offset_malloc(size_t size, size_t alignment, size_t offset)
{
	void* memptr;

	if (posix_memalign(&memptr, alignment, size) != 0)
		return NULL;

	return memptr;
}

void* _aligned_offset_realloc(void* memblock, size_t size, size_t alignment, size_t offset)
{
	return NULL;
}

void* _aligned_offset_recalloc(void* memblock, size_t num, size_t size, size_t alignment, size_t offset)
{
	return NULL;
}

size_t _aligned_msize(void* memblock, size_t alignment, size_t offset)
{
	return 0;
}

void _aligned_free(void* memblock)
{
	free(memblock);
}

#endif
