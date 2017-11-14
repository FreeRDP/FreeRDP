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

#include <winpr/spec.h>
#include <winpr/string.h>
#include <winpr/heap.h>

#ifndef _WIN32

#ifndef _strtoui64
#define _strtoui64 strtoull
#endif

#ifndef _strtoi64
#define _strtoi64 strtoll
#endif

#ifndef _rotl
static INLINE UINT32 _rotl(UINT32 value, int shift)
{
	return (value << shift) | (value >> (32 - shift));
}
#endif

#ifndef _rotl64
static INLINE UINT64 _rotl64(UINT64 value, int shift)
{
	return (value << shift) | (value >> (64 - shift));
}
#endif

#ifndef _rotr
static INLINE UINT32 _rotr(UINT32 value, int shift)
{
	return (value >> shift) | (value << (32 - shift));
}
#endif

#ifndef _rotr64
static INLINE UINT64 _rotr64(UINT64 value, int shift)
{
	return (value >> shift) | (value << (64 - shift));
}
#endif

#if (__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 2))

#define _byteswap_ulong(_val)	__builtin_bswap32(_val)
#define _byteswap_uint64(_val)	__builtin_bswap64(_val)

#else

static INLINE UINT32 _byteswap_ulong(UINT32 _val)
{
	return (((_val) >> 24) | \
	        (((_val) & 0x00FF0000) >> 8) | \
	        (((_val) & 0x0000FF00) << 8) | \
	        ((_val) << 24));
}

static INLINE UINT64 _byteswap_uint64(UINT64 _val)
{
	return (((_val) << 56) | \
	        (((_val) << 40) & 0xFF000000000000) | \
	        (((_val) << 24) & 0xFF0000000000) | \
	        (((_val) << 8)  & 0xFF00000000) | \
	        (((_val) >> 8)  & 0xFF000000) | \
	        (((_val) >> 24) & 0xFF0000) | \
	        (((_val) >> 40) & 0xFF00) | \
	        ((_val)  >> 56));
}

#endif

#if (__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 8))

#define _byteswap_ushort(_val)	__builtin_bswap16(_val)

#else

static INLINE UINT16 _byteswap_ushort(UINT16 _val)
{
	return (((_val) >> 8) | ((_val) << 8));
}

#endif



#define CopyMemory(Destination, Source, Length)		memcpy((Destination), (Source), (Length))
#define MoveMemory(Destination, Source, Length)		memmove((Destination), (Source), (Length))
#define	FillMemory(Destination, Length, Fill)		memset((Destination), (Fill), (Length))
#define ZeroMemory(Destination, Length)			memset((Destination), 0, (Length))

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API PVOID SecureZeroMemory(PVOID ptr, SIZE_T cnt);

#ifdef __cplusplus
}
#endif

/* Data Alignment */

#ifndef _ERRNO_T_DEFINED
#define _ERRNO_T_DEFINED
typedef int errno_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API void* _aligned_malloc(size_t size, size_t alignment);
WINPR_API void* _aligned_realloc(void* memblock, size_t size, size_t alignment);
WINPR_API void* _aligned_recalloc(void* memblock, size_t num, size_t size, size_t alignment);

WINPR_API void* _aligned_offset_malloc(size_t size, size_t alignment, size_t offset);
WINPR_API void* _aligned_offset_realloc(void* memblock, size_t size, size_t alignment,
                                        size_t offset);
WINPR_API void* _aligned_offset_recalloc(void* memblock, size_t num, size_t size, size_t alignment,
        size_t offset);

WINPR_API size_t _aligned_msize(void* memblock, size_t alignment, size_t offset);

WINPR_API void _aligned_free(void* memblock);

/* Data Conversion */

WINPR_API errno_t _itoa_s(int value, char* buffer, size_t sizeInCharacters, int radix);

/* Buffer Manipulation */

WINPR_API errno_t memmove_s(void* dest, size_t numberOfElements, const void* src, size_t count);
WINPR_API errno_t wmemmove_s(WCHAR* dest, size_t numberOfElements, const WCHAR* src, size_t count);

#ifdef __cplusplus
}
#endif


#endif

#endif /* WINPR_CRT_H */
