/*
 * WinPR: Windows Portable Runtime
 * Stream Utils
 *
 * Copyright 2011 Vic Lee
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

WINPR_API void Stream_EnsureCapacity(wStream* s, size_t size);
WINPR_API void Stream_EnsureRemainingCapacity(wStream* s, size_t size);

WINPR_API wStream* Stream_New(BYTE* buffer, size_t size);
WINPR_API void Stream_Free(wStream* s, BOOL bFreeBuffer);

#define _stream_read_n8(_t, _s, _v, _p) do { _v = \
	(_t)(*_s->pointer); \
	_s->pointer += _p; } while (0)

#define _stream_read_n16_le(_t, _s, _v, _p) do { _v = \
	(_t)(*_s->pointer) + \
	(((_t)(*(_s->pointer + 1))) << 8); \
	if (_p) _s->pointer += 2; } while (0)

#define _stream_read_n16_be(_t, _s, _v, _p) do { _v = \
	(((_t)(*_s->pointer)) << 8) + \
	(_t)(*(_s->pointer + 1)); \
	if (_p) _s->pointer += 2; } while (0)

#define _stream_read_n32_le(_t, _s, _v, _p) do { _v = \
	(_t)(*_s->pointer) + \
	(((_t)(*(_s->pointer + 1))) << 8) + \
	(((_t)(*(_s->pointer + 2))) << 16) + \
	(((_t)(*(_s->pointer + 3))) << 24); \
	if (_p) _s->pointer += 4; } while (0)

#define _stream_read_n32_be(_t, _s, _v, _p) do { _v = \
	(((_t)(*(_s->pointer))) << 24) + \
	(((_t)(*(_s->pointer + 1))) << 16) + \
	(((_t)(*(_s->pointer + 2))) << 8) + \
	(((_t)(*(_s->pointer + 3)))); \
	if (_p) _s->pointer += 4; } while (0)

#define _stream_read_n64_le(_t, _s, _v, _p) do { _v = \
	(_t)(*_s->pointer) + \
	(((_t)(*(_s->pointer + 1))) << 8) + \
	(((_t)(*(_s->pointer + 2))) << 16) + \
	(((_t)(*(_s->pointer + 3))) << 24) + \
	(((_t)(*(_s->pointer + 4))) << 32) + \
	(((_t)(*(_s->pointer + 5))) << 40) + \
	(((_t)(*(_s->pointer + 6))) << 48) + \
	(((_t)(*(_s->pointer + 7))) << 56); \
	if (_p) _s->pointer += 8; } while (0)

#define _stream_read_n64_be(_t, _s, _v, _p) do { _v = \
	(((_t)(*(_s->pointer))) << 56) + \
	(((_t)(*(_s->pointer + 1))) << 48) + \
	(((_t)(*(_s->pointer + 2))) << 40) + \
	(((_t)(*(_s->pointer + 3))) << 32) + \
	(((_t)(*(_s->pointer + 4))) << 24) + \
	(((_t)(*(_s->pointer + 5))) << 16) + \
	(((_t)(*(_s->pointer + 6))) << 8) + \
	(((_t)(*(_s->pointer + 7)))); \
	if (_p) _s->pointer += 8; } while (0)


#define Stream_Read_UINT8(_s, _v) _stream_read_n8(UINT8, _s, _v, 1)
#define Stream_Read_INT8(_s, _v) _stream_read_n8(INT8, _s, _v, 1)

#define Stream_Read_UINT16(_s, _v) _stream_read_n16_le(UINT16, _s, _v, 1)
#define Stream_Read_INT16(_s, _v) _stream_read_n16_le(INT16, _s, _v, 1)

#define Stream_Read_UINT16_BE(_s, _v) _stream_read_n16_be(UINT16, _s, _v, 1)
#define Stream_Read_INT16_BE(_s, _v) _stream_read_n16_be(INT16, _s, _v, 1)

#define Stream_Read_UINT32(_s, _v) _stream_read_n32_le(UINT32, _s, _v, 1)
#define Stream_Read_INT32(_s, _v) _stream_read_n32_le(INT32, _s, _v, 1)

#define Stream_Read_UINT32_BE(_s, _v) _stream_read_n32_be(UINT32, _s, _v, 1)
#define Stream_Read_INT32_BE(_s, _v) _stream_read_n32_be(INT32, _s, _v, 1)

#define Stream_Read_UINT64(_s, _v) _stream_read_n64_le(UINT64, _s, _v, 1)
#define Stream_Read_INT64(_s, _v) _stream_read_n64_le(INT64, _s, _v, 1)

#define Stream_Read_UINT64_BE(_s, _v) _stream_read_n64_be(UINT64, _s, _v, 1)
#define Stream_Read_INT64_BE(_s, _v) _stream_read_n64_be(INT64, _s, _v, 1)

#define Stream_Read(_s, _b, _n) do { \
	memcpy(_b, (_s->pointer), (_n)); \
	_s->pointer += (_n); \
	} while (0)


#define Stream_Peek_UINT8(_s, _v) _stream_read_n8(UINT8, _s, _v, 0)
#define Stream_Peek_INT8(_s, _v) _stream_read_n8(INT8, _s, _v, 0)

#define Stream_Peek_UINT16(_s, _v) _stream_read_n16_le(UINT16, _s, _v, 0)
#define Stream_Peek_INT16(_s, _v) _stream_read_n16_le(INT16, _s, _v, 0)

#define Stream_Peek_UINT16_BE(_s, _v) _stream_read_n16_be(UINT16, _s, _v, 0)
#define Stream_Peek_INT16_BE(_s, _v) _stream_read_n16_be(INT16, _s, _v, 0)

#define Stream_Peek_UINT32(_s, _v) _stream_read_n32_le(UINT32, _s, _v, 0)
#define Stream_Peek_INT32(_s, _v) _stream_read_n32_le(INT32, _s, _v, 0)

#define Stream_Peek_UINT32_BE(_s, _v) _stream_read_n32_be(UINT32, _s, _v, 0)
#define Stream_Peek_INT32_BE(_s, _v) _stream_read_n32_be(INT32, _s, _v, 0)

#define Stream_Peek_UINT64(_s, _v) _stream_read_n64_le(UINT64, _s, _v, 0)
#define Stream_Peek_INT64(_s, _v) _stream_read_n64_le(INT64, _s, _v, 0)

#define Stream_Peek_UINT64_BE(_s, _v) _stream_read_n64_be(UINT64, _s, _v, 0)
#define Stream_Peek_INT64_BE(_s, _v) _stream_read_n64_be(INT64, _s, _v, 0)

#define Stream_Peek(_s, _b, _n) do { \
	memcpy(_b, (_s->pointer), (_n)); \
	} while (0)


#define Stream_Write_UINT8(_s, _v) do { \
	*_s->pointer++ = (UINT8)(_v); } while (0)

#define Stream_Write_UINT16(_s, _v) do { \
	*_s->pointer++ = (_v) & 0xFF; \
	*_s->pointer++ = ((_v) >> 8) & 0xFF; } while (0)

#define Stream_Write_UINT16_BE(_s, _v) do { \
	*_s->pointer++ = ((_v) >> 8) & 0xFF; \
	*_s->pointer++ = (_v) & 0xFF; } while (0)

#define Stream_Write_UINT32(_s, _v) do { \
	*_s->pointer++ = (_v) & 0xFF; \
	*_s->pointer++ = ((_v) >> 8) & 0xFF; \
	*_s->pointer++ = ((_v) >> 16) & 0xFF; \
	*_s->pointer++ = ((_v) >> 24) & 0xFF; } while (0)

#define Stream_Write_UINT32_BE(_s, _v) do { \
	Stream_Write_UINT16_BE(_s, ((_v) >> 16 & 0xFFFF)); \
	Stream_Write_UINT16_BE(_s, ((_v) & 0xFFFF)); \
	} while (0)

#define Stream_Write_UINT64(_s, _v) do { \
	*_s->pointer++ = (UINT64)(_v) & 0xFF; \
	*_s->pointer++ = ((UINT64)(_v) >> 8) & 0xFF; \
	*_s->pointer++ = ((UINT64)(_v) >> 16) & 0xFF; \
	*_s->pointer++ = ((UINT64)(_v) >> 24) & 0xFF; \
	*_s->pointer++ = ((UINT64)(_v) >> 32) & 0xFF; \
	*_s->pointer++ = ((UINT64)(_v) >> 40) & 0xFF; \
	*_s->pointer++ = ((UINT64)(_v) >> 48) & 0xFF; \
	*_s->pointer++ = ((UINT64)(_v) >> 56) & 0xFF; } while (0)

#define Stream_Write(_s, _b, _n) do { \
	memcpy(_s->pointer, (_b), (_n)); \
	_s->pointer += (_n); \
	} while (0)


#define Stream_Seek(_s,_offset)		_s->pointer += (_offset)
#define Stream_Rewind(_s,_offset)	_s->pointer -= (_offset)

#define Stream_Seek_UINT8(_s)		Stream_Seek(_s, 1)
#define Stream_Seek_UINT16(_s)		Stream_Seek(_s, 2)
#define Stream_Seek_UINT32(_s)		Stream_Seek(_s, 4)
#define Stream_Seek_UINT64(_s)		Stream_Seek(_s, 8)

#define Stream_Rewind_UINT8(_s)		Stream_Rewind(_s, 1)
#define Stream_Rewind_UINT16(_s)	Stream_Rewind(_s, 2)
#define Stream_Rewind_UINT32(_s)	Stream_Rewind(_s, 4)
#define Stream_Rewind_UINT64(_s)	Stream_Rewind(_s, 8)

#define Stream_Zero(_s, _n) do { \
	memset(_s->pointer, '\0', (_n)); \
	_s->pointer += (_n); \
	} while (0)

#define Stream_Fill(_s, _v, _n) do { \
	memset(_s->pointer, _v, (_n)); \
	_s->pointer += (_n); \
	} while (0)

#define Stream_Copy(_dst, _src, _n) do { \
	memcpy(_dst->pointer, _src->pointer, _n); \
	_dst->pointer += _n; \
	_src->pointer += _n; \
	} while (0)

#define Stream_Buffer(_s)			_s->buffer
#define Stream_GetBuffer(_s, _b)	_b = _s->buffer
#define Stream_SetBuffer(_s, _b)	_s->buffer = _b

#define Stream_Pointer(_s)			_s->pointer
#define Stream_GetPointer(_s, _p)	_p = _s->pointer
#define Stream_SetPointer(_s, _p)	_s->pointer = _p

#define Stream_Length(_s)			_s->length
#define Stream_GetLength(_s, _l)	_l = _s->length
#define Stream_SetLength(_s, _l)	_s->length = _l

#define Stream_Capacity(_s)			_s->capacity
#define Stream_GetCapacity(_s, _c)	_c = _s->capacity
#define Stream_SetCapacity(_s, _c)	_s->capacity = _c

#define Stream_GetPosition(_s)		(_s->pointer - _s->buffer)
#define Stream_SetPosition(_s, _p)	_s->pointer = _s->buffer + (_p)

#define Stream_SealLength(_s)		_s->length = (_s->pointer - _s->buffer)
#define Stream_GetRemainingLength(_s)	(_s->length - (_s->pointer - _s->buffer))

#define Stream_Clear(_s)		memset(_s->buffer, 0, _s->capacity)

static INLINE BOOL Stream_SafeSeek(wStream* s, size_t size) {
	if (Stream_GetRemainingLength(s) < size)
		return FALSE;
	Stream_Seek(s, size);
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
