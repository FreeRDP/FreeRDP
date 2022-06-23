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

#include <winpr/config.h>

#include <winpr/crt.h>

/* Data Alignment: http://msdn.microsoft.com/en-us/library/fs9stz4e/ */

#if !defined(_WIN32) || (defined(__MINGW32__) && !defined(_UCRT))

#include <stdint.h>
#include <limits.h>

#define WINPR_ALIGNED_MEM_SIGNATURE 0x0BA0BAB

#define WINPR_ALIGNED_MEM_STRUCT_FROM_PTR(_memptr) \
	(WINPR_ALIGNED_MEM*)(((size_t)(((BYTE*)_memptr) - sizeof(WINPR_ALIGNED_MEM))))

#include <stdlib.h>

#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
#include <stdlib.h>
#else
#include <malloc.h>
#endif

#include "../log.h"
#define TAG WINPR_TAG("crt")

struct winpr_aligned_mem
{
	UINT32 sig;
	size_t size;
	void* base_addr;
};
typedef struct winpr_aligned_mem WINPR_ALIGNED_MEM;

void* winpr_aligned_malloc(size_t size, size_t alignment)
{
	return winpr_aligned_offset_malloc(size, alignment, 0);
}

void* winpr_aligned_realloc(void* memblock, size_t size, size_t alignment)
{
	return winpr_aligned_offset_realloc(memblock, size, alignment, 0);
}

void* winpr_aligned_recalloc(void* memblock, size_t num, size_t size, size_t alignment)
{
	return winpr_aligned_offset_recalloc(memblock, num, size, alignment, 0);
}

void* winpr_aligned_offset_malloc(size_t size, size_t alignment, size_t offset)
{
	size_t header, alignsize;
	uintptr_t basesize;
	void* base;
	void* memblock;
	WINPR_ALIGNED_MEM* pMem;

	/* alignment must be a power of 2 */
	if (alignment % 2 == 1)
		return NULL;

	/* offset must be less than size */
	if (offset >= size)
		return NULL;

	/* minimum alignment is pointer size */
	if (alignment < sizeof(void*))
		alignment = sizeof(void*);

	if (alignment > SIZE_MAX - sizeof(WINPR_ALIGNED_MEM))
		return NULL;

	header = sizeof(WINPR_ALIGNED_MEM) + alignment;

	if (size > SIZE_MAX - header)
		return NULL;

	alignsize = size + header;
	/* malloc size + alignment to make sure we can align afterwards */
	base = malloc(alignsize);

	if (!base)
		return NULL;

	basesize = (uintptr_t)base;

	if ((offset > UINTPTR_MAX) || (header > UINTPTR_MAX - offset) ||
	    (basesize > UINTPTR_MAX - header - offset))
	{
		free(base);
		return NULL;
	}

	memblock = (void*)(((basesize + header + offset) & ~(alignment - 1)) - offset);
	pMem = WINPR_ALIGNED_MEM_STRUCT_FROM_PTR(memblock);
	pMem->sig = WINPR_ALIGNED_MEM_SIGNATURE;
	pMem->base_addr = base;
	pMem->size = size;
	return memblock;
}

void* winpr_aligned_offset_realloc(void* memblock, size_t size, size_t alignment, size_t offset)
{
	size_t copySize;
	void* newMemblock;
	WINPR_ALIGNED_MEM* pMem;
	WINPR_ALIGNED_MEM* pNewMem;

	if (!memblock)
		return winpr_aligned_offset_malloc(size, alignment, offset);

	pMem = WINPR_ALIGNED_MEM_STRUCT_FROM_PTR(memblock);

	if (pMem->sig != WINPR_ALIGNED_MEM_SIGNATURE)
	{
		WLog_ERR(TAG,
		         "_aligned_offset_realloc: memory block was not allocated by _aligned_malloc!");
		return NULL;
	}

	if (size == 0)
	{
		winpr_aligned_free(memblock);
		return NULL;
	}

	newMemblock = winpr_aligned_offset_malloc(size, alignment, offset);

	if (!newMemblock)
		return NULL;

	pNewMem = WINPR_ALIGNED_MEM_STRUCT_FROM_PTR(newMemblock);
	copySize = (pNewMem->size < pMem->size) ? pNewMem->size : pMem->size;
	CopyMemory(newMemblock, memblock, copySize);
	winpr_aligned_free(memblock);
	return newMemblock;
}

static INLINE size_t cMIN(size_t a, size_t b)
{
	if (a > b)
		return b;
	return a;
}

void* winpr_aligned_offset_recalloc(void* memblock, size_t num, size_t size, size_t alignment,
                                    size_t offset)
{
	char* newMemblock = NULL;
	WINPR_ALIGNED_MEM* pMem = NULL;
	WINPR_ALIGNED_MEM* pNewMem = NULL;

	if (!memblock)
	{
		newMemblock = winpr_aligned_offset_malloc(size * num, alignment, offset);

		if (newMemblock)
		{
			pNewMem = WINPR_ALIGNED_MEM_STRUCT_FROM_PTR(newMemblock);
			ZeroMemory(newMemblock, pNewMem->size);
		}

		return newMemblock;
	}

	pMem = WINPR_ALIGNED_MEM_STRUCT_FROM_PTR(memblock);

	if (pMem->sig != WINPR_ALIGNED_MEM_SIGNATURE)
	{
		WLog_ERR(TAG,
		         "_aligned_offset_recalloc: memory block was not allocated by _aligned_malloc!");
		goto fail;
	}

	if (size == 0)
		goto fail;

	newMemblock = winpr_aligned_offset_malloc(size * num, alignment, offset);

	if (!newMemblock)
		goto fail;

	pNewMem = WINPR_ALIGNED_MEM_STRUCT_FROM_PTR(newMemblock);
	{
		const size_t csize = cMIN(pMem->size, pNewMem->size);
		memcpy(newMemblock, pMem->base_addr, csize);
		ZeroMemory(newMemblock + csize, pNewMem->size - csize);
	}
fail:
	winpr_aligned_free(memblock);
	return newMemblock;
}

size_t winpr_aligned_msize(void* memblock, size_t alignment, size_t offset)
{
	WINPR_ALIGNED_MEM* pMem;

	if (!memblock)
		return 0;

	pMem = WINPR_ALIGNED_MEM_STRUCT_FROM_PTR(memblock);

	if (pMem->sig != WINPR_ALIGNED_MEM_SIGNATURE)
	{
		WLog_ERR(TAG, "_aligned_msize: memory block was not allocated by _aligned_malloc!");
		return 0;
	}

	return pMem->size;
}

void winpr_aligned_free(void* memblock)
{
	WINPR_ALIGNED_MEM* pMem;

	if (!memblock)
		return;

	pMem = WINPR_ALIGNED_MEM_STRUCT_FROM_PTR(memblock);

	if (pMem->sig != WINPR_ALIGNED_MEM_SIGNATURE)
	{
		WLog_ERR(TAG, "_aligned_free: memory block was not allocated by _aligned_malloc!");
		return;
	}

	free(pMem->base_addr);
}

#endif /* _WIN32 */
