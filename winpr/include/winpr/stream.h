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
#include <winpr/assert.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct s_wStreamPool wStreamPool;

	typedef struct
	{
		BYTE* buffer;
		BYTE* pointer;
		size_t length;
		size_t capacity;

		DWORD count;
		wStreamPool* pool;
		BOOL isAllocatedStream;
		BOOL isOwner;
	} wStream;

	static INLINE size_t Stream_Capacity(const wStream* _s);
	WINPR_API size_t Stream_GetRemainingCapacity(const wStream* _s);
	WINPR_API size_t Stream_GetRemainingLength(const wStream* _s);

	WINPR_API BOOL Stream_EnsureCapacity(wStream* s, size_t size);
	WINPR_API BOOL Stream_EnsureRemainingCapacity(wStream* s, size_t size);

	WINPR_API wStream* Stream_New(BYTE* buffer, size_t size);
	WINPR_API wStream* Stream_StaticConstInit(wStream* s, const BYTE* buffer, size_t size);
	WINPR_API wStream* Stream_StaticInit(wStream* s, BYTE* buffer, size_t size);
	WINPR_API void Stream_Free(wStream* s, BOOL bFreeBuffer);

#define Stream_CheckAndLogRequiredLength(tag, s, len)                                     \
	Stream_CheckAndLogRequiredLengthEx(tag, WLOG_WARN, s, len, "%s(%s:%d)", __FUNCTION__, \
	                                   __FILE__, __LINE__)
	WINPR_API BOOL Stream_CheckAndLogRequiredLengthEx(const char* tag, DWORD level, wStream* s,
	                                                  UINT64 len, const char* fmt, ...);
	WINPR_API BOOL Stream_CheckAndLogRequiredLengthExVa(const char* tag, DWORD level, wStream* s,
	                                                    UINT64 len, const char* fmt, va_list args);
	WINPR_API BOOL Stream_CheckAndLogRequiredLengthWLogEx(wLog* log, DWORD level, wStream* s,
	                                                      UINT64 len, const char* fmt, ...);
	WINPR_API BOOL Stream_CheckAndLogRequiredLengthWLogExVa(wLog* log, DWORD level, wStream* s,
	                                                        UINT64 len, const char* fmt,
	                                                        va_list args);

	static INLINE void Stream_Seek(wStream* s, size_t _offset)
	{
		WINPR_ASSERT(s);
		WINPR_ASSERT(Stream_GetRemainingCapacity(s) >= _offset);
		s->pointer += (_offset);
	}

	static INLINE void Stream_Rewind(wStream* s, size_t _offset)
	{
		size_t cur;
		WINPR_ASSERT(s);
		WINPR_ASSERT(s->buffer <= s->pointer);
		cur = (size_t)(s->pointer - s->buffer);
		WINPR_ASSERT(cur >= _offset);
		if (cur >= _offset)
			s->pointer -= (_offset);
		else
			s->pointer = s->buffer;
	}

#define _stream_read_n8(_t, _s, _v, _p)                       \
	do                                                        \
	{                                                         \
		WINPR_ASSERT(_s);                                     \
		if (_p)                                               \
		{                                                     \
			WINPR_ASSERT(Stream_GetRemainingLength(_s) >= 1); \
		}                                                     \
		(_v) = (_t)(*(_s)->pointer);                          \
		if (_p)                                               \
			Stream_Seek(_s, sizeof(_t));                      \
	} while (0)

#define _stream_read_n16_le(_t, _s, _v, _p)                                      \
	do                                                                           \
	{                                                                            \
		WINPR_ASSERT(_s);                                                        \
		if (_p)                                                                  \
		{                                                                        \
			WINPR_ASSERT(Stream_GetRemainingLength(_s) >= 2);                    \
		}                                                                        \
		(_v) = (_t)((*(_s)->pointer) + (((UINT16)(*((_s)->pointer + 1))) << 8)); \
		if (_p)                                                                  \
			Stream_Seek(_s, sizeof(_t));                                         \
	} while (0)

#define _stream_read_n16_be(_t, _s, _v, _p)                                              \
	do                                                                                   \
	{                                                                                    \
		WINPR_ASSERT(_s);                                                                \
		if (_p)                                                                          \
		{                                                                                \
			WINPR_ASSERT(Stream_GetRemainingLength(_s) >= 2);                            \
		}                                                                                \
		(_v) = (_t)((((UINT16)(*(_s)->pointer)) << 8) + (UINT16)(*((_s)->pointer + 1))); \
		if (_p)                                                                          \
			Stream_Seek(_s, sizeof(_t));                                                 \
	} while (0)

#define _stream_read_n32_le(_t, _s, _v, _p)                                              \
	do                                                                                   \
	{                                                                                    \
		WINPR_ASSERT(_s);                                                                \
		if (_p)                                                                          \
		{                                                                                \
			WINPR_ASSERT(Stream_GetRemainingLength(_s) >= 4);                            \
		}                                                                                \
		(_v) = (_t)((UINT32)(*(_s)->pointer) + (((UINT32)(*((_s)->pointer + 1))) << 8) + \
		            (((UINT32)(*((_s)->pointer + 2))) << 16) +                           \
		            ((((UINT32) * ((_s)->pointer + 3))) << 24));                         \
		if (_p)                                                                          \
			Stream_Seek(_s, sizeof(_t));                                                 \
	} while (0)

#define _stream_read_n32_be(_t, _s, _v, _p)                                                        \
	do                                                                                             \
	{                                                                                              \
		WINPR_ASSERT(_s);                                                                          \
		if (_p)                                                                                    \
		{                                                                                          \
			WINPR_ASSERT(Stream_GetRemainingLength(_s) >= 4);                                      \
		}                                                                                          \
		(_v) = (_t)(((((UINT32) * ((_s)->pointer))) << 24) +                                       \
		            (((UINT32)(*((_s)->pointer + 1))) << 16) +                                     \
		            (((UINT32)(*((_s)->pointer + 2))) << 8) + (((UINT32)(*((_s)->pointer + 3))))); \
		if (_p)                                                                                    \
			Stream_Seek(_s, sizeof(_t));                                                           \
	} while (0)

#define _stream_read_n64_le(_t, _s, _v, _p)                                                       \
	do                                                                                            \
	{                                                                                             \
		WINPR_ASSERT(_s);                                                                         \
		if (_p)                                                                                   \
		{                                                                                         \
			WINPR_ASSERT(Stream_GetRemainingLength(_s) >= 8);                                     \
		}                                                                                         \
		(_v) = (_t)(                                                                              \
		    (UINT64)(*(_s)->pointer) + (((UINT64)(*((_s)->pointer + 1))) << 8) +                  \
		    (((UINT64)(*((_s)->pointer + 2))) << 16) + (((UINT64)(*((_s)->pointer + 3))) << 24) + \
		    (((UINT64)(*((_s)->pointer + 4))) << 32) + (((UINT64)(*((_s)->pointer + 5))) << 40) + \
		    (((UINT64)(*((_s)->pointer + 6))) << 48) + (((UINT64)(*((_s)->pointer + 7))) << 56)); \
		if (_p)                                                                                   \
			Stream_Seek(_s, sizeof(_t));                                                          \
	} while (0)

#define _stream_read_n64_be(_t, _s, _v, _p)                                                       \
	do                                                                                            \
	{                                                                                             \
		WINPR_ASSERT(_s);                                                                         \
		if (_p)                                                                                   \
		{                                                                                         \
			WINPR_ASSERT(Stream_GetRemainingLength(_s) >= 8);                                     \
		}                                                                                         \
		(_v) = (_t)(                                                                              \
		    (((UINT64)(*((_s)->pointer))) << 56) + (((UINT64)(*((_s)->pointer + 1))) << 48) +     \
		    (((UINT64)(*((_s)->pointer + 2))) << 40) + (((UINT64)(*((_s)->pointer + 3))) << 32) + \
		    (((UINT64)(*((_s)->pointer + 4))) << 24) + (((UINT64)(*((_s)->pointer + 5))) << 16) + \
		    (((UINT64)(*((_s)->pointer + 6))) << 8) + (((UINT64)(*((_s)->pointer + 7)))));        \
		if (_p)                                                                                   \
			Stream_Seek(_s, sizeof(_t));                                                          \
	} while (0)

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
		WINPR_ASSERT(_s);
		WINPR_ASSERT(_b || (_n == 0));
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= _n);
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

	static INLINE void Stream_Peek(const wStream* _s, void* _b, size_t _n)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(_b || (_n == 0));
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= _n);
		memcpy(_b, (_s->pointer), (_n));
	}

	static INLINE void Stream_Write_UINT8(wStream* _s, UINT8 _v)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 1);
		*_s->pointer++ = (UINT8)(_v);
	}

	static INLINE void Stream_Write_INT16(wStream* _s, INT16 _v)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 2);
		*_s->pointer++ = (_v)&0xFF;
		*_s->pointer++ = ((_v) >> 8) & 0xFF;
	}

	static INLINE void Stream_Write_UINT16(wStream* _s, UINT16 _v)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 2);
		*_s->pointer++ = (_v)&0xFF;
		*_s->pointer++ = ((_v) >> 8) & 0xFF;
	}

	static INLINE void Stream_Write_UINT16_BE(wStream* _s, UINT16 _v)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 2);
		*_s->pointer++ = ((_v) >> 8) & 0xFF;
		*_s->pointer++ = (_v)&0xFF;
	}

	static INLINE void Stream_Write_UINT24_BE(wStream* _s, UINT32 _v)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 3);
		*_s->pointer++ = ((_v) >> 16) & 0xFF;
		*_s->pointer++ = ((_v) >> 8) & 0xFF;
		*_s->pointer++ = (_v)&0xFF;
	}

	static INLINE void Stream_Write_INT32(wStream* _s, INT32 _v)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 4);
		*_s->pointer++ = (_v)&0xFF;
		*_s->pointer++ = ((_v) >> 8) & 0xFF;
		*_s->pointer++ = ((_v) >> 16) & 0xFF;
		*_s->pointer++ = ((_v) >> 24) & 0xFF;
	}

	static INLINE void Stream_Write_UINT32(wStream* _s, UINT32 _v)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 4);
		*_s->pointer++ = (_v)&0xFF;
		*_s->pointer++ = ((_v) >> 8) & 0xFF;
		*_s->pointer++ = ((_v) >> 16) & 0xFF;
		*_s->pointer++ = ((_v) >> 24) & 0xFF;
	}

	static INLINE void Stream_Write_UINT32_BE(wStream* _s, UINT32 _v)
	{
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 4);
		Stream_Write_UINT16_BE(_s, ((_v) >> 16 & 0xFFFF));
		Stream_Write_UINT16_BE(_s, ((_v)&0xFFFF));
	}

	static INLINE void Stream_Write_UINT64(wStream* _s, UINT64 _v)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 8);
		*_s->pointer++ = (UINT64)(_v)&0xFF;
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
		if (_n > 0)
		{
			WINPR_ASSERT(_s);
			WINPR_ASSERT(_b);
			WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= _n);
			memcpy(_s->pointer, (_b), (_n));
			Stream_Seek(_s, _n);
		}
	}

#define Stream_Seek_UINT8(_s) Stream_Seek(_s, 1)
#define Stream_Seek_UINT16(_s) Stream_Seek(_s, 2)
#define Stream_Seek_UINT32(_s) Stream_Seek(_s, 4)
#define Stream_Seek_UINT64(_s) Stream_Seek(_s, 8)

#define Stream_Rewind_UINT8(_s) Stream_Rewind(_s, 1)
#define Stream_Rewind_UINT16(_s) Stream_Rewind(_s, 2)
#define Stream_Rewind_UINT32(_s) Stream_Rewind(_s, 4)
#define Stream_Rewind_UINT64(_s) Stream_Rewind(_s, 8)

	static INLINE void Stream_Zero(wStream* _s, size_t _n)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= (_n));
		memset(_s->pointer, '\0', (_n));
		Stream_Seek(_s, _n);
	}

	static INLINE void Stream_Fill(wStream* _s, int _v, size_t _n)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= (_n));
		memset(_s->pointer, _v, (_n));
		Stream_Seek(_s, _n);
	}

	static INLINE void Stream_Copy(wStream* _src, wStream* _dst, size_t _n)
	{
		WINPR_ASSERT(_src);
		WINPR_ASSERT(_dst);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_src) >= (_n));
		WINPR_ASSERT(Stream_GetRemainingCapacity(_dst) >= (_n));

		memcpy(_dst->pointer, _src->pointer, _n);
		Stream_Seek(_dst, _n);
		Stream_Seek(_src, _n);
	}

	static INLINE BYTE* Stream_Buffer(wStream* _s)
	{
		WINPR_ASSERT(_s);
		return _s->buffer;
	}

	static INLINE const BYTE* Stream_ConstBuffer(const wStream* _s)
	{
		WINPR_ASSERT(_s);
		return _s->buffer;
	}

#define Stream_GetBuffer(_s, _b) _b = Stream_Buffer(_s)
#define Stream_PointerAs(s, type) (type*)Stream_Pointer(s)

	static INLINE BYTE* Stream_Pointer(wStream* _s)
	{
		WINPR_ASSERT(_s);
		return _s->pointer;
	}

	static INLINE const BYTE* Stream_ConstPointer(const wStream* _s)
	{
		WINPR_ASSERT(_s);
		return _s->pointer;
	}

#define Stream_GetPointer(_s, _p) _p = Stream_Pointer(_s)

#if defined(WITH_WINPR_DEPRECATED)
	WINPR_API WINPR_DEPRECATED_VAR("Use Stream_SetPosition instead",
	                               BOOL Stream_SetPointer(wStream* _s, BYTE* _p));
	WINPR_API WINPR_DEPRECATED_VAR("Use Stream_New(buffer, capacity) instead",
	                               BOOL Stream_SetBuffer(wStream* _s, BYTE* _b));
	WINPR_API WINPR_DEPRECATED_VAR("Use Stream_New(buffer, capacity) instead",
	                               void Stream_SetCapacity(wStream* _s, size_t capacity));
#endif

	static INLINE size_t Stream_Length(const wStream* _s)
	{
		WINPR_ASSERT(_s);
		return _s->length;
	}

#define Stream_GetLength(_s, _l) _l = Stream_Length(_s)
	WINPR_API BOOL Stream_SetLength(wStream* _s, size_t _l);

	static INLINE size_t Stream_Capacity(const wStream* _s)
	{
		WINPR_ASSERT(_s);
		return _s->capacity;
	}

#define Stream_GetCapacity(_s, _c) _c = Stream_Capacity(_s);

	static INLINE size_t Stream_GetPosition(const wStream* _s)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(_s->buffer <= _s->pointer);
		return (size_t)(_s->pointer - _s->buffer);
	}

	WINPR_API BOOL Stream_SetPosition(wStream* _s, size_t _p);

	WINPR_API void Stream_SealLength(wStream* _s);

	static INLINE void Stream_Clear(wStream* _s)
	{
		WINPR_ASSERT(_s);
		memset(_s->buffer, 0, _s->capacity);
	}

	static INLINE BOOL Stream_SafeSeek(wStream* s, size_t size)
	{
		if (Stream_GetRemainingLength(s) < size)
			return FALSE;

		Stream_Seek(s, size);
		return TRUE;
	}

	WINPR_API BOOL Stream_Read_UTF16_String(wStream* s, WCHAR* dst, size_t length);
	WINPR_API BOOL Stream_Write_UTF16_String(wStream* s, const WCHAR* src, size_t length);

	/* StreamPool */

	WINPR_API wStream* StreamPool_Take(wStreamPool* pool, size_t size);
	WINPR_API void StreamPool_Return(wStreamPool* pool, wStream* s);

	WINPR_API void Stream_AddRef(wStream* s);
	WINPR_API void Stream_Release(wStream* s);

	WINPR_API wStream* StreamPool_Find(wStreamPool* pool, BYTE* ptr);

	WINPR_API void StreamPool_Clear(wStreamPool* pool);

	WINPR_API wStreamPool* StreamPool_New(BOOL synchronized, size_t defaultSize);
	WINPR_API void StreamPool_Free(wStreamPool* pool);

	WINPR_API char* StreamPool_GetStatistics(wStreamPool* pool, char* buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_UTILS_STREAM_H */
