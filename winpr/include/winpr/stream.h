/*
 * WinPR: Windows Portable Runtime
 * Stream Utils
 *
 * Copyright 2011 Vic Lee
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2017 Armin Novak <armin.novak@thincast.com>
 * Copyright 2017 Thincast Technologies GmbH
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

#ifndef WINPR_UTILS_STREAM_H
#define WINPR_UTILS_STREAM_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>
#include <winpr/endian.h>
#include <winpr/synch.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _wStreamPool wStreamPool;

struct _wStream
{
	BYTE* buffer;
	BYTE* pointer;
	size_t length;
	size_t capacity;

	DWORD count;
	wStreamPool* pool;
};
typedef struct _wStream wStream;

WINPR_API BOOL Stream_EnsureCapacity(wStream* s, size_t size);
WINPR_API BOOL Stream_EnsureRemainingCapacity(wStream* s, size_t size);

WINPR_API wStream* Stream_New(BYTE* buffer, size_t size);
WINPR_API void Stream_Free(wStream* s, BOOL bFreeBuffer);

static INLINE void Stream_Seek(wStream* s, size_t _offset)
{
	s->pointer += (_offset);
}

static INLINE void Stream_Rewind(wStream* s, size_t _offset)
{
	s->pointer -= (_offset);
}

#define _stream_read_n8(_t, _s, _v, _p) do { \
		_v = \
		     (_t)(*_s->pointer); \
		if (_p) Stream_Seek(_s, sizeof(_t)); } while (0)

#define _stream_read_n16_le(_t, _s, _v, _p) do { \
		_v = \
		     (_t)(*_s->pointer) + \
		     (((_t)(*(_s->pointer + 1))) << 8); \
		if (_p) Stream_Seek(_s, sizeof(_t)); } while (0)

#define _stream_read_n16_be(_t, _s, _v, _p) do { \
		_v = \
		     (((_t)(*_s->pointer)) << 8) + \
		     (_t)(*(_s->pointer + 1)); \
		if (_p) Stream_Seek(_s, sizeof(_t)); } while (0)

#define _stream_read_n32_le(_t, _s, _v, _p) do { \
		_v = \
		     (_t)(*_s->pointer) + \
		     (((_t)(*(_s->pointer + 1))) << 8) + \
		     (((_t)(*(_s->pointer + 2))) << 16) + \
		     (((_t)(*(_s->pointer + 3))) << 24); \
		if (_p) Stream_Seek(_s, sizeof(_t)); } while (0)

#define _stream_read_n32_be(_t, _s, _v, _p) do { \
		_v = \
		     (((_t)(*(_s->pointer))) << 24) + \
		     (((_t)(*(_s->pointer + 1))) << 16) + \
		     (((_t)(*(_s->pointer + 2))) << 8) + \
		     (((_t)(*(_s->pointer + 3)))); \
		if (_p) Stream_Seek(_s, sizeof(_t)); } while (0)

#define _stream_read_n64_le(_t, _s, _v, _p) do { \
		_v = \
		     (_t)(*_s->pointer) + \
		     (((_t)(*(_s->pointer + 1))) << 8) + \
		     (((_t)(*(_s->pointer + 2))) << 16) + \
		     (((_t)(*(_s->pointer + 3))) << 24) + \
		     (((_t)(*(_s->pointer + 4))) << 32) + \
		     (((_t)(*(_s->pointer + 5))) << 40) + \
		     (((_t)(*(_s->pointer + 6))) << 48) + \
		     (((_t)(*(_s->pointer + 7))) << 56); \
		if (_p) Stream_Seek(_s, sizeof(_t)); } while (0)

#define _stream_read_n64_be(_t, _s, _v, _p) do { \
		_v = \
		     (((_t)(*(_s->pointer))) << 56) + \
		     (((_t)(*(_s->pointer + 1))) << 48) + \
		     (((_t)(*(_s->pointer + 2))) << 40) + \
		     (((_t)(*(_s->pointer + 3))) << 32) + \
		     (((_t)(*(_s->pointer + 4))) << 24) + \
		     (((_t)(*(_s->pointer + 5))) << 16) + \
		     (((_t)(*(_s->pointer + 6))) << 8) + \
		     (((_t)(*(_s->pointer + 7)))); \
		if (_p) Stream_Seek(_s, sizeof(_t)); } while (0)

#define Stream_Read_UINT8(_s, _v) _stream_read_n8(UINT8, _s, _v, TRUE)
#define Stream_Read_INT8(_s, _v) _stream_read_n8(INT8, _s, _v, TRUE)

#define Stream_Read_UINT16(_s, _v) _stream_read_n16_le(UINT16, _s, _v, TRUE)
#define Stream_Read_INT16(_s, _v) _stream_read_n16_le(INT16, _s, _v, TRUE)

#define Stream_Read_UINT16_BE(_s, _v) _stream_read_n16_be(UINT16, _s, _v, TRUE)
#define Stream_Read_INT16_BE(_s, _v) _stream_read_n16_be(INT16, _s, _v, TRUE)

#define Stream_Read_UINT32(_s, _v) _stream_read_n32_le(UINT32, _s, _v, TRUE)
#define Stream_Read_INT32(_s, _v) _stream_read_n32_le(INT32, _s, _v, TRUE)

#define Stream_Read_UINT32_BE(_s, _v) _stream_read_n32_be(UINT32, _s, _v, TRUE)
#define Stream_Read_INT32_BE(_s, _v) _stream_read_n32_be(INT32, _s, _v, TRUE)

#define Stream_Read_UINT64(_s, _v) _stream_read_n64_le(UINT64, _s, _v, TRUE)
#define Stream_Read_INT64(_s, _v) _stream_read_n64_le(INT64, _s, _v, TRUE)

#define Stream_Read_UINT64_BE(_s, _v) _stream_read_n64_be(UINT64, _s, _v, TRUE)
#define Stream_Read_INT64_BE(_s, _v) _stream_read_n64_be(INT64, _s, _v, TRUE)

static INLINE void Stream_Read(wStream* _s, void* _b, size_t _n)
{
	memcpy(_b, (_s->pointer), (_n));
	Stream_Seek(_s, _n);
}

#define Stream_Peek_UINT8(_s, _v) _stream_read_n8(UINT8, _s, _v, FALSE)
#define Stream_Peek_INT8(_s, _v) _stream_read_n8(INT8, _s, _v, FALSE)

#define Stream_Peek_UINT16(_s, _v) _stream_read_n16_le(UINT16, _s, _v, FALSE)
#define Stream_Peek_INT16(_s, _v) _stream_read_n16_le(INT16, _s, _v, FALSE)

#define Stream_Peek_UINT16_BE(_s, _v) _stream_read_n16_be(UINT16, _s, _v, FALSE)
#define Stream_Peek_INT16_BE(_s, _v) _stream_read_n16_be(INT16, _s, _v, FALSE)

#define Stream_Peek_UINT32(_s, _v) _stream_read_n32_le(UINT32, _s, _v, FALSE)
#define Stream_Peek_INT32(_s, _v) _stream_read_n32_le(INT32, _s, _v, FALSE)

#define Stream_Peek_UINT32_BE(_s, _v) _stream_read_n32_be(UINT32, _s, _v, FALSE)
#define Stream_Peek_INT32_BE(_s, _v) _stream_read_n32_be(INT32, _s, _v, FALSE)

#define Stream_Peek_UINT64(_s, _v) _stream_read_n64_le(UINT64, _s, _v, FALSE)
#define Stream_Peek_INT64(_s, _v) _stream_read_n64_le(INT64, _s, _v, FALSE)

#define Stream_Peek_UINT64_BE(_s, _v) _stream_read_n64_be(UINT64, _s, _v, FALSE)
#define Stream_Peek_INT64_BE(_s, _v) _stream_read_n64_be(INT64, _s, _v, FALSE)

static INLINE void Stream_Peek(wStream* _s, void* _b, size_t _n)
{
	memcpy(_b, (_s->pointer), (_n));
}

static INLINE void Stream_Write_UINT8(wStream* _s, UINT8 _v)
{
	*_s->pointer++ = (UINT8)(_v);
}

static INLINE void Stream_Write_UINT16(wStream* _s, UINT16 _v)
{
	*_s->pointer++ = (_v) & 0xFF;
	*_s->pointer++ = ((_v) >> 8) & 0xFF;
}

static INLINE void Stream_Write_UINT16_BE(wStream* _s, UINT16 _v)
{
	*_s->pointer++ = ((_v) >> 8) & 0xFF;
	*_s->pointer++ = (_v) & 0xFF;
}

static INLINE void Stream_Write_UINT32(wStream* _s, UINT32 _v)
{
	*_s->pointer++ = (_v) & 0xFF;
	*_s->pointer++ = ((_v) >> 8) & 0xFF;
	*_s->pointer++ = ((_v) >> 16) & 0xFF;
	*_s->pointer++ = ((_v) >> 24) & 0xFF;
}

static INLINE void Stream_Write_UINT32_BE(wStream* _s, UINT32 _v)
{
	Stream_Write_UINT16_BE(_s, ((_v) >> 16 & 0xFFFF));
	Stream_Write_UINT16_BE(_s, ((_v) & 0xFFFF));
}

static INLINE void Stream_Write_UINT64(wStream* _s, UINT64 _v)
{
	*_s->pointer++ = (UINT64)(_v) & 0xFF;
	*_s->pointer++ = ((UINT64)(_v) >> 8) & 0xFF;
	*_s->pointer++ = ((UINT64)(_v) >> 16) & 0xFF;
	*_s->pointer++ = ((UINT64)(_v) >> 24) & 0xFF;
	*_s->pointer++ = ((UINT64)(_v) >> 32) & 0xFF;
	*_s->pointer++ = ((UINT64)(_v) >> 40) & 0xFF;
	*_s->pointer++ = ((UINT64)(_v) >> 48) & 0xFF;
	*_s->pointer++ = ((UINT64)(_v) >> 56) & 0xFF;
}
static INLINE void Stream_Write(wStream* _s, const void* _b, size_t _n)
{
	memcpy(_s->pointer, (_b), (_n));
	Stream_Seek(_s, _n);
}

#define Stream_Seek_UINT8(_s)		Stream_Seek(_s, 1)
#define Stream_Seek_UINT16(_s)		Stream_Seek(_s, 2)
#define Stream_Seek_UINT32(_s)		Stream_Seek(_s, 4)
#define Stream_Seek_UINT64(_s)		Stream_Seek(_s, 8)

#define Stream_Rewind_UINT8(_s)		Stream_Rewind(_s, 1)
#define Stream_Rewind_UINT16(_s)	Stream_Rewind(_s, 2)
#define Stream_Rewind_UINT32(_s)	Stream_Rewind(_s, 4)
#define Stream_Rewind_UINT64(_s)	Stream_Rewind(_s, 8)

static INLINE void Stream_Zero(wStream* _s, size_t _n)
{
	memset(_s->pointer, '\0', (_n));
	Stream_Seek(_s, _n);
}

static INLINE void Stream_Fill(wStream* _s, int _v, size_t _n)
{
	memset(_s->pointer, _v, (_n));
	Stream_Seek(_s, _n);
}

static INLINE void Stream_Copy(wStream* _src, wStream* _dst, size_t _n)
{
	memcpy(_dst->pointer, _src->pointer, _n);
	Stream_Seek(_dst, _n);
	Stream_Seek(_src, _n);
}

static INLINE BYTE* Stream_Buffer(wStream* _s)
{
	return _s->buffer;
}

#define Stream_GetBuffer(_s, _b)	_b = Stream_Buffer(_s)
static INLINE void Stream_SetBuffer(wStream* _s, BYTE* _b)
{
	_s->buffer = _b;
}

static INLINE BYTE* Stream_Pointer(wStream* _s)
{
	return _s->pointer;
}

#define Stream_GetPointer(_s, _p)	_p = Stream_Pointer(_s)
static INLINE void Stream_SetPointer(wStream* _s, BYTE* _p)
{
	_s->pointer = _p;
}

static INLINE size_t Stream_Length(wStream* _s)
{
	return _s->length;
}

#define Stream_GetLength(_s, _l)	_l = Stream_Length(_s)
static INLINE void Stream_SetLength(wStream* _s, size_t _l)
{
	_s->length = _l;
}

static INLINE size_t Stream_Capacity(wStream* _s)
{
	return _s->capacity;
}

#define Stream_GetCapacity(_s, _c)	_c = Stream_Capacity(_s);
static INLINE void Stream_SetCapacity(wStream* _s, size_t _c)
{
	_s->capacity = _c;
}

static INLINE size_t Stream_GetPosition(wStream* _s)
{
	return (_s->pointer - _s->buffer);
}

static INLINE void Stream_SetPosition(wStream* _s, size_t _p)
{
	_s->pointer = _s->buffer + (_p);
}

static INLINE void Stream_SealLength(wStream* _s)
{
	_s->length = (_s->pointer - _s->buffer);
}

static INLINE size_t Stream_GetRemainingCapacity(wStream* _s)
{
	return (_s->capacity - (_s->pointer - _s->buffer));
}

static INLINE size_t Stream_GetRemainingLength(wStream* _s)
{
	return (_s->length - (_s->pointer - _s->buffer));
}

static INLINE void Stream_Clear(wStream* _s)
{
	memset(_s->buffer, 0, _s->capacity);
}

static INLINE BOOL Stream_SafeSeek(wStream* s, size_t size)
{
	if (Stream_GetRemainingLength(s) < size)
		return FALSE;

	Stream_Seek(s, size);
	return TRUE;
}

static INLINE BOOL Stream_Read_UTF16_String(wStream* s, WCHAR* dst, size_t length)
{
	size_t x;

	if (!s || !dst)
		return FALSE;

	if (Stream_GetRemainingLength(s) / sizeof(WCHAR) < length)
		return FALSE;

	for (x=0; x<length; x++)
		Stream_Read_UINT16(s, dst[x]);

	return TRUE;
}

static INLINE BOOL Stream_Write_UTF16_String(wStream* s, const WCHAR* src, size_t length)
{
	size_t x;

	if (!s || !src)
		return FALSE;

	if (Stream_GetRemainingCapacity(s) / sizeof(WCHAR) < length)
		return FALSE;

	for (x=0; x<length; x++)
		Stream_Write_UINT16(s, src[x]);

	return TRUE;
}

/* StreamPool */

struct _wStreamPool
{
	int aSize;
	int aCapacity;
	wStream** aArray;

	int uSize;
	int uCapacity;
	wStream** uArray;

	CRITICAL_SECTION lock;
	BOOL synchronized;
	size_t defaultSize;
};

WINPR_API wStream* StreamPool_Take(wStreamPool* pool, size_t size);
WINPR_API void StreamPool_Return(wStreamPool* pool, wStream* s);

WINPR_API void Stream_AddRef(wStream* s);
WINPR_API void Stream_Release(wStream* s);

WINPR_API wStream* StreamPool_Find(wStreamPool* pool, BYTE* ptr);
WINPR_API void StreamPool_AddRef(wStreamPool* pool, BYTE* ptr);
WINPR_API void StreamPool_Release(wStreamPool* pool, BYTE* ptr);

WINPR_API void StreamPool_Clear(wStreamPool* pool);

WINPR_API wStreamPool* StreamPool_New(BOOL synchronized, size_t defaultSize);
WINPR_API void StreamPool_Free(wStreamPool* pool);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_UTILS_STREAM_H */
