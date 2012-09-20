/**
 * WinPR: Windows Portable Runtime
 * C Run-Time Library Routines
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

#ifndef WINPR_CRT_H
#define WINPR_CRT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/winpr.h>

#include <winpr/string.h>
#include <winpr/memory.h>

/* Data Alignment */

#ifndef _WIN32

WINPR_API void* _aligned_malloc(size_t size, size_t alignment);
WINPR_API void* _aligned_realloc(void* memblock, size_t size, size_t alignment);
WINPR_API void* _aligned_recalloc(void* memblock, size_t num, size_t size, size_t alignment);

WINPR_API void* _aligned_offset_malloc(size_t size, size_t alignment, size_t offset);
WINPR_API void* _aligned_offset_realloc(void* memblock, size_t size, size_t alignment, size_t offset);
WINPR_API void* _aligned_offset_recalloc(void* memblock, size_t num, size_t size, size_t alignment, size_t offset);

WINPR_API size_t _aligned_msize(void* memblock, size_t alignment, size_t offset);

WINPR_API void _aligned_free(void* memblock);

#endif

#endif /* WINPR_CRT_H */
