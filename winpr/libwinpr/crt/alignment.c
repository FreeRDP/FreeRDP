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

#ifdef __APPLE__
#include <malloc/malloc.h>
#elif __FreeBSD__
#include <stdlib.h>
#else
#include <malloc.h>
#endif

struct _aligned_meminfo
{
	size_t size;
	void* base_addr;
};

void* _aligned_malloc(size_t size, size_t alignment)
{
	return _aligned_offset_malloc(size, alignment, 0);
}

void* _aligned_realloc(void* memblock, size_t size, size_t alignment)
{
	return _aligned_offset_realloc(memblock, size, alignment, 0);
}

void* _aligned_recalloc(void* memblock, size_t num, size_t size, size_t alignment)
{
	return NULL;
}

void* _aligned_offset_malloc(size_t size, size_t alignment, size_t offset)
{
	void* memptr, *tmpptr;
	struct _aligned_meminfo *ameminfo;

	/* alignment must be a power of 2 */
	if (alignment % 2 == 1)
		return NULL;

	/* offset must be less than size */
	if (offset >= size)
		return NULL;

	/* minimum alignment is pointer size */
	if (alignment < sizeof(void*))
		alignment = sizeof(void*);

	/* malloc size + alignment to make sure we can align afterwards */
	tmpptr = malloc(size + alignment + sizeof(struct _aligned_meminfo));

	if (!tmpptr)
		return NULL;

	memptr = (void *)((((size_t)((PBYTE)tmpptr + alignment + offset + sizeof(struct _aligned_meminfo)) & ~(alignment - 1)) - offset));

	ameminfo = (struct _aligned_meminfo*) (((size_t)((PBYTE)memptr - sizeof(struct _aligned_meminfo))));
	ameminfo->base_addr = tmpptr;
	ameminfo->size = size;

	return memptr;
}

void* _aligned_offset_realloc(void* memblock, size_t size, size_t alignment, size_t offset)
{
	void* newmem;
	struct _aligned_meminfo* ameminfo;

	if (!memblock)
		return _aligned_offset_malloc(size, alignment, offset);

	if (size == 0)
	{
		_aligned_free(memblock);
		return NULL;
	}

	/*  The following is not very performant but a simple and working solution */
	newmem = _aligned_offset_malloc(size, alignment, offset);

	if (!newmem)
		return NULL;

	ameminfo = (struct _aligned_meminfo*) (((size_t)((PBYTE)memblock - sizeof(struct _aligned_meminfo))));
	CopyMemory(newmem, memblock, ameminfo->size);
	_aligned_free(memblock);

	return newmem;
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
	struct _aligned_meminfo* ameminfo;

	if (!memblock)
		return;

	ameminfo = (struct _aligned_meminfo*) (((size_t)((PBYTE)memblock - sizeof(struct _aligned_meminfo))));

	free(ameminfo->base_addr);
}

#endif
