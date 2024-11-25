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

#ifdef __cplusplus
#define WINPR_STREAM_CAST(t, val) static_cast<t>(val)
#else
#define WINPR_STREAM_CAST(t, val) (t)(val)
#endif

#define Stream_CheckAndLogRequiredCapacityOfSize(tag, s, nmemb, size)                         \
	Stream_CheckAndLogRequiredCapacityEx(tag, WLOG_WARN, s, nmemb, size, "%s(%s:%" PRIuz ")", \
	                                     __func__, __FILE__, (size_t)__LINE__)
#define Stream_CheckAndLogRequiredCapacity(tag, s, len) \
	Stream_CheckAndLogRequiredCapacityOfSize((tag), (s), (len), 1)

	WINPR_API BOOL Stream_CheckAndLogRequiredCapacityEx(const char* tag, DWORD level, wStream* s,
	                                                    size_t nmemb, size_t size, const char* fmt,
	                                                    ...);
	WINPR_API BOOL Stream_CheckAndLogRequiredCapacityExVa(const char* tag, DWORD level, wStream* s,
	                                                      size_t nmemb, size_t size,
	                                                      const char* fmt, va_list args);

#define Stream_CheckAndLogRequiredCapacityOfSizeWLog(log, s, nmemb, size)                         \
	Stream_CheckAndLogRequiredCapacityWLogEx(log, WLOG_WARN, s, nmemb, size, "%s(%s:%" PRIuz ")", \
	                                         __func__, __FILE__, (size_t)__LINE__)

#define Stream_CheckAndLogRequiredCapacityWLog(log, s, len) \
	Stream_CheckAndLogRequiredCapacityOfSizeWLog((log), (s), (len), 1)

	WINPR_API BOOL Stream_CheckAndLogRequiredCapacityWLogEx(wLog* log, DWORD level, wStream* s,
	                                                        size_t nmemb, size_t size,
	                                                        const char* fmt, ...);
	WINPR_API BOOL Stream_CheckAndLogRequiredCapacityWLogExVa(wLog* log, DWORD level, wStream* s,
	                                                          size_t nmemb, size_t size,
	                                                          const char* fmt, va_list args);

	WINPR_API void Stream_Free(wStream* s, BOOL bFreeBuffer);

	WINPR_ATTR_MALLOC(Stream_Free, 1)
	WINPR_API wStream* Stream_New(BYTE* buffer, size_t size);
	WINPR_API wStream* Stream_StaticConstInit(wStream* s, const BYTE* buffer, size_t size);
	WINPR_API wStream* Stream_StaticInit(wStream* s, BYTE* buffer, size_t size);

#define Stream_CheckAndLogRequiredLengthOfSize(tag, s, nmemb, size)                         \
	Stream_CheckAndLogRequiredLengthEx(tag, WLOG_WARN, s, nmemb, size, "%s(%s:%" PRIuz ")", \
	                                   __func__, __FILE__, (size_t)__LINE__)
#define Stream_CheckAndLogRequiredLength(tag, s, len) \
	Stream_CheckAndLogRequiredLengthOfSize(tag, s, len, 1)

	WINPR_API BOOL Stream_CheckAndLogRequiredLengthEx(const char* tag, DWORD level, wStream* s,
	                                                  size_t nmemb, size_t size, const char* fmt,
	                                                  ...);
	WINPR_API BOOL Stream_CheckAndLogRequiredLengthExVa(const char* tag, DWORD level, wStream* s,
	                                                    size_t nmemb, size_t size, const char* fmt,
	                                                    va_list args);

#define Stream_CheckAndLogRequiredLengthOfSizeWLog(log, s, nmemb, size)                         \
	Stream_CheckAndLogRequiredLengthWLogEx(log, WLOG_WARN, s, nmemb, size, "%s(%s:%" PRIuz ")", \
	                                       __func__, __FILE__, (size_t)__LINE__)
#define Stream_CheckAndLogRequiredLengthWLog(log, s, len) \
	Stream_CheckAndLogRequiredLengthOfSizeWLog(log, s, len, 1)

	WINPR_API BOOL Stream_CheckAndLogRequiredLengthWLogEx(wLog* log, DWORD level, wStream* s,
	                                                      size_t nmemb, size_t size,
	                                                      const char* fmt, ...);
	WINPR_API BOOL Stream_CheckAndLogRequiredLengthWLogExVa(wLog* log, DWORD level, wStream* s,
	                                                        size_t nmemb, size_t size,
	                                                        const char* fmt, va_list args);

	static INLINE void Stream_Seek(wStream* s, size_t _offset)
	{
		WINPR_ASSERT(s);
		WINPR_ASSERT(Stream_GetRemainingCapacity(s) >= _offset);
		s->pointer += (_offset);
	}

	static INLINE void Stream_Rewind(wStream* s, size_t _offset)
	{
		size_t cur = 0;
		WINPR_ASSERT(s);
		WINPR_ASSERT(s->buffer <= s->pointer);
		cur = WINPR_STREAM_CAST(size_t, s->pointer - s->buffer);
		WINPR_ASSERT(cur >= _offset);
		if (cur >= _offset)
			s->pointer -= (_offset);
		else
			s->pointer = s->buffer;
	}

	static INLINE UINT8 stream_read_u8(wStream* _s, BOOL seek)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(Stream_GetRemainingLength(_s) >= sizeof(UINT8));

		const UINT8 v = *(_s)->pointer;
		if (seek)
			Stream_Seek(_s, sizeof(UINT8));
		return v;
	}

	static INLINE INT8 stream_read_i8(wStream* _s, BOOL seek)
	{
		const UINT8 v = stream_read_u8(_s, seek);
		return WINPR_STREAM_CAST(INT8, v);
	}

	static INLINE UINT16 stream_read_u16_le(wStream* _s, BOOL seek)
	{
		const size_t typesize = sizeof(UINT16);
		WINPR_ASSERT(_s);
		WINPR_ASSERT(Stream_GetRemainingLength(_s) >= typesize);

		UINT16 v = 0;
		for (size_t x = 0; x < typesize; x++)
		{
			v <<= 8;
			v |= (_s)->pointer[typesize - x - 1];
		}
		if (seek)
			Stream_Seek(_s, typesize);
		return v;
	}

	static INLINE UINT16 stream_read_u16_be(wStream* _s, BOOL seek)
	{
		const size_t typesize = sizeof(UINT16);
		WINPR_ASSERT(_s);
		WINPR_ASSERT(Stream_GetRemainingLength(_s) >= typesize);

		UINT16 v = 0;
		for (size_t x = 0; x < typesize; x++)
		{
			v <<= 8;
			v |= (_s)->pointer[x];
		}
		if (seek)
			Stream_Seek(_s, typesize);
		return v;
	}

	static INLINE INT16 stream_read_i16_le(wStream* _s, BOOL seek)
	{
		const UINT16 v = stream_read_u16_le(_s, seek);
		return WINPR_STREAM_CAST(INT16, v);
	}

	static INLINE INT16 stream_read_i16_be(wStream* _s, BOOL seek)
	{
		const UINT16 v = stream_read_u16_be(_s, seek);
		return WINPR_STREAM_CAST(INT16, v);
	}

	static INLINE UINT32 stream_read_u32_le(wStream* _s, BOOL seek)
	{
		const size_t typesize = sizeof(UINT32);
		WINPR_ASSERT(_s);
		WINPR_ASSERT(Stream_GetRemainingLength(_s) >= typesize);

		UINT32 v = 0;
		for (size_t x = 0; x < typesize; x++)
		{
			v <<= 8;
			v |= (_s)->pointer[typesize - x - 1];
		}
		if (seek)
			Stream_Seek(_s, typesize);
		return v;
	}

	static INLINE UINT32 stream_read_u32_be(wStream* _s, BOOL seek)
	{
		const size_t typesize = sizeof(UINT32);
		WINPR_ASSERT(_s);
		WINPR_ASSERT(Stream_GetRemainingLength(_s) >= typesize);

		UINT32 v = 0;
		for (size_t x = 0; x < typesize; x++)
		{
			v <<= 8;
			v |= (_s)->pointer[x];
		}
		if (seek)
			Stream_Seek(_s, typesize);
		return v;
	}

	static INLINE INT32 stream_read_i32_le(wStream* _s, BOOL seek)
	{
		const UINT32 v = stream_read_u32_le(_s, seek);
		return WINPR_STREAM_CAST(INT32, v);
	}

	static INLINE INT32 stream_read_i32_be(wStream* _s, BOOL seek)
	{
		const UINT32 v = stream_read_u32_be(_s, seek);
		return WINPR_STREAM_CAST(INT32, v);
	}

	static INLINE UINT64 stream_read_u64_le(wStream* _s, BOOL seek)
	{
		const size_t typesize = sizeof(UINT64);
		WINPR_ASSERT(_s);
		WINPR_ASSERT(Stream_GetRemainingLength(_s) >= typesize);

		UINT64 v = 0;
		for (size_t x = 0; x < typesize; x++)
		{
			v <<= 8;
			v |= (_s)->pointer[typesize - x - 1];
		}
		if (seek)
			Stream_Seek(_s, typesize);
		return v;
	}

	static INLINE UINT64 stream_read_u64_be(wStream* _s, BOOL seek)
	{
		const size_t typesize = sizeof(UINT64);
		WINPR_ASSERT(_s);
		WINPR_ASSERT(Stream_GetRemainingLength(_s) >= typesize);

		UINT64 v = 0;
		for (size_t x = 0; x < typesize; x++)
		{
			v <<= 8;
			v |= (_s)->pointer[x];
		}
		if (seek)
			Stream_Seek(_s, typesize);
		return v;
	}

	static INLINE INT64 stream_read_i64_le(wStream* _s, BOOL seek)
	{
		const UINT64 v = stream_read_u64_le(_s, seek);
		return WINPR_STREAM_CAST(INT64, v);
	}

	static INLINE INT64 stream_read_i64_be(wStream* _s, BOOL seek)
	{
		const UINT64 v = stream_read_u64_be(_s, seek);
		return WINPR_STREAM_CAST(INT64, v);
	}

	/**
	 * @brief Stream_Get_UINT8
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE UINT8 Stream_Get_UINT8(wStream* _s)
	{
		return stream_read_u8(_s, TRUE);
	}

	/**
	 * @brief Stream_Get_INT8
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE INT8 Stream_Get_INT8(wStream* _s)
	{
		return stream_read_i8(_s, TRUE);
	}

	/**
	 * @brief Stream_Get_UINT16
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE UINT16 Stream_Get_UINT16(wStream* _s)
	{
		return stream_read_u16_le(_s, TRUE);
	}

	/**
	 * @brief Stream_Get_INT16
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE INT16 Stream_Get_INT16(wStream* _s)
	{
		return stream_read_i16_le(_s, TRUE);
	}

	/**
	 * @brief Stream_Get_UINT16 big endian
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE UINT16 Stream_Get_UINT16_BE(wStream* _s)
	{
		return stream_read_u16_be(_s, TRUE);
	}

	/**
	 * @brief Stream_Get_INT16 big endian
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE INT16 Stream_Get_INT16_BE(wStream* _s)
	{
		return stream_read_i16_be(_s, TRUE);
	}

	/**
	 * @brief Stream_Get_UINT32
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE UINT32 Stream_Get_UINT32(wStream* _s)
	{
		return stream_read_u32_le(_s, TRUE);
	}

	/**
	 * @brief Stream_Get_INT32
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE INT32 Stream_Get_INT32(wStream* _s)
	{
		return stream_read_i32_le(_s, TRUE);
	}

	/**
	 * @brief Stream_Get_UINT32 big endian
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE UINT32 Stream_Get_UINT32_BE(wStream* _s)
	{
		return stream_read_u32_be(_s, TRUE);
	}

	/**
	 * @brief Stream_Get_INT32 big endian
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE INT32 Stream_Get_INT32_BE(wStream* _s)
	{
		return stream_read_i32_be(_s, TRUE);
	}

	/**
	 * @brief Stream_Get_UINT64
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE UINT64 Stream_Get_UINT64(wStream* _s)
	{
		return stream_read_u64_le(_s, TRUE);
	}

	/**
	 * @brief Stream_Get_INT64
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE INT64 Stream_Get_INT64(wStream* _s)
	{
		return stream_read_i64_le(_s, TRUE);
	}

	/**
	 * @brief Stream_Get_UINT64 big endian
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE UINT64 Stream_Get_UINT64_BE(wStream* _s)
	{
		return stream_read_u64_be(_s, TRUE);
	}

	/**
	 * @brief Stream_Get_INT64 big endian
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE INT64 Stream_Get_INT64_BE(wStream* _s)
	{
		return stream_read_i64_be(_s, TRUE);
	}

	/**
	 * @brief Read a UINT8 from the stream, do not increment stream position
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE UINT8 Stream_Peek_Get_UINT8(wStream* _s)
	{
		return stream_read_u8(_s, FALSE);
	}

	/**
	 * @brief Read a INT8 from the stream, do not increment stream position
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE INT8 Stream_Peek_Get_INT8(wStream* _s)
	{
		return stream_read_i8(_s, FALSE);
	}

	/**
	 * @brief Read a UINT16 from the stream, do not increment stream position
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE UINT16 Stream_Peek_Get_UINT16(wStream* _s)
	{
		return stream_read_u16_le(_s, FALSE);
	}

	/**
	 * @brief Read a INT16 from the stream, do not increment stream position
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE INT16 Stream_Peek_Get_INT16(wStream* _s)
	{
		return stream_read_i16_le(_s, FALSE);
	}

	/**
	 * @brief Read a UINT16 big endian from the stream, do not increment stream position
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE UINT16 Stream_Peek_Get_UINT16_BE(wStream* _s)
	{
		return stream_read_u16_be(_s, FALSE);
	}

	/**
	 * @brief Read a INT16 big endian from the stream, do not increment stream position
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE INT16 Stream_Peek_Get_INT16_BE(wStream* _s)
	{
		return stream_read_i16_be(_s, FALSE);
	}

	/**
	 * @brief Read a UINT32 from the stream, do not increment stream position
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE UINT32 Stream_Peek_Get_UINT32(wStream* _s)
	{
		return stream_read_u32_le(_s, FALSE);
	}

	/**
	 * @brief Read a INT32 from the stream, do not increment stream position
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE INT32 Stream_Peek_Get_INT32(wStream* _s)
	{
		return stream_read_i32_le(_s, FALSE);
	}

	/**
	 * @brief Read a UINT32 big endian from the stream, do not increment stream position
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE UINT32 Stream_Peek_Get_UINT32_BE(wStream* _s)
	{
		return stream_read_u32_be(_s, FALSE);
	}

	/**
	 * @brief Read a INT32 big endian from the stream, do not increment stream position
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE INT32 Stream_Peek_Get_INT32_BE(wStream* _s)
	{
		return stream_read_i32_be(_s, FALSE);
	}

	/**
	 * @brief Read a UINT64 from the stream, do not increment stream position
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE UINT64 Stream_Peek_Get_UINT64(wStream* _s)
	{
		return stream_read_u64_le(_s, FALSE);
	}

	/**
	 * @brief Read a INT64 from the stream, do not increment stream position
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE INT64 Stream_Peek_Get_INT64(wStream* _s)
	{
		return stream_read_i64_le(_s, FALSE);
	}

	/**
	 * @brief Read a UINT64 big endian from the stream, do not increment stream position
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE UINT64 Stream_Peek_Get_UINT64_BE(wStream* _s)
	{
		return stream_read_u64_be(_s, FALSE);
	}

	/**
	 * @brief Read a INT64 big endian from the stream, do not increment stream position
	 * @param _s The stream to read from
	 * @return an integer
	 * @since version 3.9.0
	 */
	static INLINE INT64 Stream_Peek_Get_INT64_BE(wStream* _s)
	{
		return stream_read_i64_be(_s, FALSE);
	}

#define Stream_Read_UINT8(_s, _v)      \
	do                                 \
	{                                  \
		_v = stream_read_u8(_s, TRUE); \
	} while (0)

#define Stream_Read_INT8(_s, _v)       \
	do                                 \
	{                                  \
		_v = stream_read_i8(_s, TRUE); \
	} while (0)

#define Stream_Read_UINT16(_s, _v)         \
	do                                     \
	{                                      \
		_v = stream_read_u16_le(_s, TRUE); \
	} while (0)

#define Stream_Read_INT16(_s, _v)          \
	do                                     \
	{                                      \
		_v = stream_read_i16_le(_s, TRUE); \
	} while (0)

#define Stream_Read_UINT16_BE(_s, _v)      \
	do                                     \
	{                                      \
		_v = stream_read_u16_be(_s, TRUE); \
	} while (0)

#define Stream_Read_INT16_BE(_s, _v)       \
	do                                     \
	{                                      \
		_v = stream_read_i16_be(_s, TRUE); \
	} while (0)

#define Stream_Read_UINT32(_s, _v)         \
	do                                     \
	{                                      \
		_v = stream_read_u32_le(_s, TRUE); \
	} while (0)

#define Stream_Read_INT32(_s, _v)          \
	do                                     \
	{                                      \
		_v = stream_read_i32_le(_s, TRUE); \
	} while (0)

#define Stream_Read_UINT32_BE(_s, _v)      \
	do                                     \
	{                                      \
		_v = stream_read_u32_be(_s, TRUE); \
	} while (0)

#define Stream_Read_INT32_BE(_s, _v)       \
	do                                     \
	{                                      \
		_v = stream_read_i32_be(_s, TRUE); \
	} while (0)

#define Stream_Read_UINT64(_s, _v)         \
	do                                     \
	{                                      \
		_v = stream_read_u64_le(_s, TRUE); \
	} while (0)

#define Stream_Read_INT64(_s, _v)          \
	do                                     \
	{                                      \
		_v = stream_read_i64_le(_s, TRUE); \
	} while (0)

#define Stream_Read_UINT64_BE(_s, _v)      \
	do                                     \
	{                                      \
		_v = stream_read_u64_be(_s, TRUE); \
	} while (0)

#define Stream_Read_INT64_BE(_s, _v)       \
	do                                     \
	{                                      \
		_v = stream_read_i64_be(_s, TRUE); \
	} while (0)

	static INLINE void Stream_Read(wStream* _s, void* _b, size_t _n)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(_b || (_n == 0));
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= _n);
		memcpy(_b, (_s->pointer), (_n));
		Stream_Seek(_s, _n);
	}

#define Stream_Peek_UINT8(_s, _v)       \
	do                                  \
	{                                   \
		_v = stream_read_u8(_s, FALSE); \
	} while (0)

#define Stream_Peek_INT8(_s, _v)        \
	do                                  \
	{                                   \
		_v = stream_read_i8(_s, FALSE); \
	} while (0)

#define Stream_Peek_UINT16(_s, _v)          \
	do                                      \
	{                                       \
		_v = stream_read_u16_le(_s, FALSE); \
	} while (0)

#define Stream_Peek_INT16(_s, _v)           \
	do                                      \
	{                                       \
		_v = stream_read_i16_le(_s, FALSE); \
	} while (0)

#define Stream_Peek_UINT16_BE(_s, _v)       \
	do                                      \
	{                                       \
		_v = stream_read_u16_be(_s, FALSE); \
	} while (0)

#define Stream_Peek_INT16_BE(_s, _v)        \
	do                                      \
	{                                       \
		_v = stream_read_i16_be(_s, FALSE); \
	} while (0)

#define Stream_Peek_UINT32(_s, _v)          \
	do                                      \
	{                                       \
		_v = stream_read_u32_le(_s, FALSE); \
	} while (0)

#define Stream_Peek_INT32(_s, _v)           \
	do                                      \
	{                                       \
		_v = stream_read_i32_le(_s, FALSE); \
	} while (0)

#define Stream_Peek_UINT32_BE(_s, _v)       \
	do                                      \
	{                                       \
		_v = stream_read_u32_be(_s, FALSE); \
	} while (0)

#define Stream_Peek_INT32_BE(_s, _v)        \
	do                                      \
	{                                       \
		_v = stream_read_i32_be(_s, FALSE); \
	} while (0)

#define Stream_Peek_UINT64(_s, _v)          \
	do                                      \
	{                                       \
		_v = stream_read_u64_le(_s, FALSE); \
	} while (0)

#define Stream_Peek_INT64(_s, _v)           \
	do                                      \
	{                                       \
		_v = stream_read_i64_le(_s, FALSE); \
	} while (0)

#define Stream_Peek_UINT64_BE(_s, _v)       \
	do                                      \
	{                                       \
		_v = stream_read_u64_be(_s, FALSE); \
	} while (0)

#define Stream_Peek_INT64_BE(_s, _v)        \
	do                                      \
	{                                       \
		_v = stream_read_i64_be(_s, FALSE); \
	} while (0)

	static INLINE void Stream_Peek(const wStream* _s, void* _b, size_t _n)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(_b || (_n == 0));
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= _n);
		memcpy(_b, (_s->pointer), (_n));
	}

#define Stream_Write_INT8(s, v)                \
	do                                         \
	{                                          \
		WINPR_ASSERT((v) <= INT8_MAX);         \
		WINPR_ASSERT((v) >= INT8_MIN);         \
		Stream_Write_INT8_unchecked((s), (v)); \
	} while (0)

	/** @brief writes a \b INT8 to a \b wStream. The stream must be large enough to hold the data.
	 *
	 * Do not use directly, use the define @ref Stream_Write_INT8 instead
	 *
	 * \param _s The stream to write to, must not be \b NULL
	 * \param _v The value to write
	 */
	static INLINE void Stream_Write_INT8_unchecked(wStream* _s, INT8 _v)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(_s->pointer);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 1);

		*_s->pointer++ = WINPR_STREAM_CAST(BYTE, _v);
	}

#define Stream_Write_UINT8(s, v)                \
	do                                          \
	{                                           \
		WINPR_ASSERT((v) <= UINT8_MAX);         \
		WINPR_ASSERT((v) >= 0);                 \
		Stream_Write_UINT8_unchecked((s), (v)); \
	} while (0)

	/** @brief writes a \b UINT8 to a \b wStream. The stream must be large enough to hold the data.
	 *
	 * Do not use directly, use the define @ref Stream_Write_UINT8 instead
	 *
	 * \param _s The stream to write to, must not be \b NULL
	 * \param _v The value to write
	 */
	static INLINE void Stream_Write_UINT8_unchecked(wStream* _s, UINT8 _v)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(_s->pointer);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 1);

		*_s->pointer++ = WINPR_STREAM_CAST(BYTE, _v);
	}

#define Stream_Write_INT16(s, v)                \
	do                                          \
	{                                           \
		WINPR_ASSERT((v) >= INT16_MIN);         \
		WINPR_ASSERT((v) <= INT16_MAX);         \
		Stream_Write_INT16_unchecked((s), (v)); \
	} while (0)

	/** @brief writes a \b INT16 as \b little endian to a \b wStream. The stream must be large
	 * enough to hold the data.
	 *
	 * Do not use directly, use the define @ref Stream_Write_INT16 instead
	 *
	 * \param _s The stream to write to, must not be \b NULL
	 * \param _v The value to write
	 */
	static INLINE void Stream_Write_INT16_unchecked(wStream* _s, INT16 _v)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(_s->pointer);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 2);

		*_s->pointer++ = (_v) & 0xFF;
		*_s->pointer++ = ((_v) >> 8) & 0xFF;
	}

#define Stream_Write_UINT16(s, v)                \
	do                                           \
	{                                            \
		WINPR_ASSERT((v) <= UINT16_MAX);         \
		WINPR_ASSERT((v) >= 0);                  \
		Stream_Write_UINT16_unchecked((s), (v)); \
	} while (0)

	/** @brief writes a \b UINT16 as \b little endian to a \b wStream. The stream must be large
	 * enough to hold the data.
	 *
	 * Do not use directly, use the define @ref Stream_Write_UINT16 instead
	 *
	 * \param _s The stream to write to, must not be \b NULL
	 * \param _v The value to write
	 */
	static INLINE void Stream_Write_UINT16_unchecked(wStream* _s, UINT16 _v)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(_s->pointer);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 2);

		*_s->pointer++ = (_v) & 0xFF;
		*_s->pointer++ = ((_v) >> 8) & 0xFF;
	}

#define Stream_Write_UINT16_BE(s, v)                \
	do                                              \
	{                                               \
		WINPR_ASSERT((v) <= UINT16_MAX);            \
		WINPR_ASSERT((v) >= 0);                     \
		Stream_Write_UINT16_BE_unchecked((s), (v)); \
	} while (0)

	/** @brief writes a \b UINT16 as \b big endian to a \b wStream. The stream must be large enough
	 * to hold the data.
	 *
	 * Do not use directly, use the define @ref Stream_Write_UINT16_BE instead
	 *
	 * \param _s The stream to write to, must not be \b NULL
	 * \param _v The value to write
	 */
	static INLINE void Stream_Write_UINT16_BE_unchecked(wStream* _s, UINT16 _v)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(_s->pointer);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 2);

		*_s->pointer++ = ((_v) >> 8) & 0xFF;
		*_s->pointer++ = (_v) & 0xFF;
	}

#define Stream_Write_INT16_BE(s, v)                \
	do                                             \
	{                                              \
		WINPR_ASSERT((v) <= INT16_MAX);            \
		WINPR_ASSERT((v) >= INT16_MIN);            \
		Stream_Write_INT16_BE_unchecked((s), (v)); \
	} while (0)

	/** @brief writes a \b UINT16 as \b big endian to a \b wStream. The stream must be large enough
	 * to hold the data.
	 *
	 * Do not use directly, use the define @ref Stream_Write_UINT16_BE instead
	 *
	 * \param _s The stream to write to, must not be \b NULL
	 * \param _v The value to write
	 *
	 * @since version 3.10.0
	 */
	static INLINE void Stream_Write_INT16_BE_unchecked(wStream* _s, INT16 _v)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(_s->pointer);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 2);

		*_s->pointer++ = ((_v) >> 8) & 0xFF;
		*_s->pointer++ = (_v) & 0xFF;
	}

#define Stream_Write_UINT24_BE(s, v)                \
	do                                              \
	{                                               \
		WINPR_ASSERT((v) <= 0xFFFFFF);              \
		WINPR_ASSERT((v) >= 0);                     \
		Stream_Write_UINT24_BE_unchecked((s), (v)); \
	} while (0)

	/** @brief writes a \b UINT24 as \b big endian to a \b wStream. The stream must be large enough
	 * to hold the data.
	 *
	 * Do not use directly, use the define @ref Stream_Write_UINT24_BE instead
	 *
	 * \param _s The stream to write to, must not be \b NULL
	 * \param _v The value to write
	 */
	static INLINE void Stream_Write_UINT24_BE_unchecked(wStream* _s, UINT32 _v)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(_s->pointer);
		WINPR_ASSERT(_v <= 0x00FFFFFF);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 3);

		*_s->pointer++ = ((_v) >> 16) & 0xFF;
		*_s->pointer++ = ((_v) >> 8) & 0xFF;
		*_s->pointer++ = (_v) & 0xFF;
	}

#define Stream_Write_INT32(s, v)                \
	do                                          \
	{                                           \
		WINPR_ASSERT((v) <= INT32_MAX);         \
		WINPR_ASSERT((v) >= INT32_MIN);         \
		Stream_Write_INT32_unchecked((s), (v)); \
	} while (0)

	/** @brief writes a \b INT32 as \b little endian to a \b wStream. The stream must be large
	 * enough to hold the data.
	 *
	 * Do not use directly, use the define @ref Stream_Write_INT32 instead
	 *
	 * \param _s The stream to write to, must not be \b NULL
	 * \param _v The value to write
	 */
	static INLINE void Stream_Write_INT32_unchecked(wStream* _s, INT32 _v)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(_s->pointer);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 4);

		*_s->pointer++ = (_v) & 0xFF;
		*_s->pointer++ = ((_v) >> 8) & 0xFF;
		*_s->pointer++ = ((_v) >> 16) & 0xFF;
		*_s->pointer++ = ((_v) >> 24) & 0xFF;
	}

#define Stream_Write_INT32_BE(s, v)                \
	do                                             \
	{                                              \
		WINPR_ASSERT((v) <= INT32_MAX);            \
		WINPR_ASSERT((v) >= INT32_MIN);            \
		Stream_Write_INT32_BE_unchecked((s), (v)); \
	} while (0)

	/** @brief writes a \b INT32 as \b little endian to a \b wStream. The stream must be large
	 * enough to hold the data.
	 *
	 * Do not use directly, use the define @ref Stream_Write_INT32 instead
	 *
	 * \param _s The stream to write to, must not be \b NULL
	 * \param _v The value to write
	 *
	 * @since version 3.10.0
	 */
	static INLINE void Stream_Write_INT32_BE_unchecked(wStream* _s, INT32 _v)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(_s->pointer);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 4);

		*_s->pointer++ = ((_v) >> 24) & 0xFF;
		*_s->pointer++ = ((_v) >> 16) & 0xFF;
		*_s->pointer++ = ((_v) >> 8) & 0xFF;
		*_s->pointer++ = (_v) & 0xFF;
	}

#define Stream_Write_UINT32(s, v)                \
	do                                           \
	{                                            \
		WINPR_ASSERT((v) <= UINT32_MAX);         \
		WINPR_ASSERT((v) >= 0);                  \
		Stream_Write_UINT32_unchecked((s), (v)); \
	} while (0)

	/** @brief writes a \b UINT32 as \b little endian to a \b wStream. The stream must be large
	 * enough to hold the data.
	 *
	 * Do not use directly, use the define @ref Stream_Write_UINT32 instead
	 *
	 * \param _s The stream to write to, must not be \b NULL
	 * \param _v The value to write
	 */
	static INLINE void Stream_Write_UINT32_unchecked(wStream* _s, UINT32 _v)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(_s->pointer);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 4);

		*_s->pointer++ = (_v) & 0xFF;
		*_s->pointer++ = ((_v) >> 8) & 0xFF;
		*_s->pointer++ = ((_v) >> 16) & 0xFF;
		*_s->pointer++ = ((_v) >> 24) & 0xFF;
	}

#define Stream_Write_UINT32_BE(s, v)                \
	do                                              \
	{                                               \
		WINPR_ASSERT((v) <= UINT32_MAX);            \
		WINPR_ASSERT((v) >= 0);                     \
		Stream_Write_UINT32_BE_unchecked((s), (v)); \
	} while (0)

	/** @brief writes a \b UINT32 as \b big endian to a \b wStream. The stream must be large enough
	 * to hold the data.
	 *
	 * Do not use directly, use the define @ref Stream_Write_UINT32_BE instead
	 *
	 * \param _s The stream to write to, must not be \b NULL
	 * \param _v The value to write
	 */
	static INLINE void Stream_Write_UINT32_BE_unchecked(wStream* _s, UINT32 _v)
	{
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 4);

		Stream_Write_UINT16_BE(_s, ((_v) >> 16 & 0xFFFF));
		Stream_Write_UINT16_BE(_s, ((_v) & 0xFFFF));
	}

	/** @brief writes a \b UINT64 as \b little endian to a \b wStream. The stream must be large
	 * enough to hold the data.
	 *
	 * \param _s The stream to write to, must not be \b NULL
	 * \param _v The value to write
	 */
	static INLINE void Stream_Write_UINT64(wStream* _s, UINT64 _v)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(_s->pointer);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 8);

		Stream_Write_UINT32(_s, ((_v) & 0xFFFFFFFFUL));
		Stream_Write_UINT32(_s, ((_v) >> 32 & 0xFFFFFFFFUL));
	}

	/** @brief writes a \b UINT64 as \b big endian to a \b wStream. The stream must be large enough
	 * to hold the data.
	 *
	 * \param _s The stream to write to, must not be \b NULL
	 * \param _v The value to write
	 */
	static INLINE void Stream_Write_UINT64_BE(wStream* _s, UINT64 _v)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(_s->pointer);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 8);

		Stream_Write_UINT32_BE(_s, ((_v) >> 32 & 0xFFFFFFFFUL));
		Stream_Write_UINT32_BE(_s, ((_v) & 0xFFFFFFFFUL));
	}

	/** @brief writes a \b INT64 as \b little endian to a \b wStream. The stream must be large
	 * enough to hold the data.
	 *
	 * \param _s The stream to write to, must not be \b NULL
	 * \param _v The value to write
	 * \since version 3.10.0
	 */
	static INLINE void Stream_Write_INT64(wStream* _s, INT64 _v)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(_s->pointer);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 8);

		*_s->pointer++ = (_v) & 0xFF;
		*_s->pointer++ = ((_v) >> 8) & 0xFF;
		*_s->pointer++ = ((_v) >> 16) & 0xFF;
		*_s->pointer++ = ((_v) >> 24) & 0xFF;
		*_s->pointer++ = ((_v) >> 32) & 0xFF;
		*_s->pointer++ = ((_v) >> 40) & 0xFF;
		*_s->pointer++ = ((_v) >> 48) & 0xFF;
		*_s->pointer++ = ((_v) >> 56) & 0xFF;
	}

	/** @brief writes a \b INT64 as \b big endian to a \b wStream. The stream must be large enough
	 * to hold the data.
	 *
	 * \param _s The stream to write to, must not be \b NULL
	 * \param _v The value to write
	 * \since version 3.10.0
	 */
	static INLINE void Stream_Write_INT64_BE(wStream* _s, INT64 _v)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(_s->pointer);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= 8);

		*_s->pointer++ = (_v >> 56) & 0xFF;
		*_s->pointer++ = ((_v) >> 48) & 0xFF;
		*_s->pointer++ = ((_v) >> 40) & 0xFF;
		*_s->pointer++ = ((_v) >> 32) & 0xFF;
		*_s->pointer++ = ((_v) >> 24) & 0xFF;
		*_s->pointer++ = ((_v) >> 16) & 0xFF;
		*_s->pointer++ = ((_v) >> 8) & 0xFF;
		*_s->pointer++ = ((_v) >> 0) & 0xFF;
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

	static INLINE void Stream_Seek_UINT8(wStream* _s)
	{
		Stream_Seek(_s, sizeof(UINT8));
	}
	static INLINE void Stream_Seek_UINT16(wStream* _s)
	{
		Stream_Seek(_s, sizeof(UINT16));
	}
	static INLINE void Stream_Seek_UINT32(wStream* _s)
	{
		Stream_Seek(_s, sizeof(UINT32));
	}
	static INLINE void Stream_Seek_UINT64(wStream* _s)
	{
		Stream_Seek(_s, sizeof(UINT64));
	}

	static INLINE void Stream_Rewind_UINT8(wStream* _s)
	{
		Stream_Rewind(_s, sizeof(UINT8));
	}
	static INLINE void Stream_Rewind_UINT16(wStream* _s)
	{
		Stream_Rewind(_s, sizeof(UINT16));
	}
	static INLINE void Stream_Rewind_UINT32(wStream* _s)
	{
		Stream_Rewind(_s, sizeof(UINT32));
	}
	static INLINE void Stream_Rewind_UINT64(wStream* _s)
	{
		Stream_Rewind(_s, sizeof(UINT64));
	}

	static INLINE void Stream_Fill(wStream* _s, int _v, size_t _n)
	{
		WINPR_ASSERT(_s);
		WINPR_ASSERT(Stream_GetRemainingCapacity(_s) >= (_n));
		memset(_s->pointer, _v, (_n));
		Stream_Seek(_s, _n);
	}

	static INLINE void Stream_Zero(wStream* _s, size_t _n)
	{
		Stream_Fill(_s, '\0', _n);
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

/** @brief Convenience macro to get a pointer to the stream buffer casted to a specific type
 *
 *  @since version 3.9.0
 */
#define Stream_BufferAs(s, type) WINPR_STREAM_CAST(type*, Stream_Buffer(s))

	static INLINE BYTE* Stream_Buffer(wStream* _s)
	{
		WINPR_ASSERT(_s);
		return _s->buffer;
	}

/** @brief Convenience macro to get a const pointer to the stream buffer casted to a specific type
 *
 *  @since version 3.9.0
 */
#define Stream_ConstBufferAs(s, type) WINPR_STREAM_CAST(type*, Stream_ConstBuffer(s))
	static INLINE const BYTE* Stream_ConstBuffer(const wStream* _s)
	{
		WINPR_ASSERT(_s);
		return _s->buffer;
	}

#define Stream_GetBuffer(_s, _b) _b = Stream_Buffer(_s)

/** @brief Convenience macro to get a pointer to the stream buffer casted to a specific type
 *
 *  @since version 3.9.0
 */
#define Stream_GetBufferAs(_s, _b) _b = Stream_BufferAs(_s, typeof(_b))

#define Stream_PointerAs(s, type) WINPR_STREAM_CAST(type*, Stream_Pointer(s))

	static INLINE void* Stream_Pointer(wStream* _s)
	{
		WINPR_ASSERT(_s);
		return _s->pointer;
	}

	static INLINE const void* Stream_ConstPointer(const wStream* _s)
	{
		WINPR_ASSERT(_s);
		return _s->pointer;
	}

#define Stream_GetPointer(_s, _p) _p = Stream_Pointer(_s)

/** @brief Convenience macro to get a pointer to the stream pointer casted to a specific type
 *
 *  @since version 3.9.0
 */
#define Stream_GetPointerAs(_s, _p) _p = Stream_PointerAs(_s, typeof(_p))

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
		return WINPR_STREAM_CAST(size_t, (_s->pointer - _s->buffer));
	}

	WINPR_API BOOL Stream_SetPosition(wStream* _s, size_t _p);

	WINPR_API void Stream_SealLength(wStream* _s);

	static INLINE void Stream_Clear(wStream* _s)
	{
		WINPR_ASSERT(_s);
		memset(_s->buffer, 0, _s->capacity);
	}

#define Stream_SafeSeek(s, size) Stream_SafeSeekEx(s, size, __FILE__, __LINE__, __func__)
	WINPR_API BOOL Stream_SafeSeekEx(wStream* s, size_t size, const char* file, size_t line,
	                                 const char* fkt);

	WINPR_API BOOL Stream_Read_UTF16_String(wStream* s, WCHAR* dst, size_t charLength);
	WINPR_API BOOL Stream_Write_UTF16_String(wStream* s, const WCHAR* src, size_t charLength);

	/** \brief Reads a WCHAR string from a stream and converts it to UTF-8 and returns a newly
	 * allocated string
	 *
	 *  \param s The stream to read data from
	 *  \param wcharLength The number of WCHAR characters to read (NOT the size in bytes!)
	 *  \param pUtfCharLength Ignored if \b NULL, otherwise will be set to the number of
	 *         characters in the resulting UTF-8 string
	 *  \return A '\0' terminated UTF-8 encoded string or NULL for any failure.
	 */
	WINPR_API char* Stream_Read_UTF16_String_As_UTF8(wStream* s, size_t wcharLength,
	                                                 size_t* pUtfCharLength);

	/** \brief Reads a WCHAR string from a stream and converts it to UTF-8 and
	 *  writes it to the supplied buffer
	 *
	 *  \param s The stream to read data from
	 *  \param wcharLength The number of WCHAR characters to read (NOT the size in bytes!)
	 *  \param utfBuffer A pointer to a buffer holding the result string
	 *  \param utfBufferCharLength The size of the result buffer
	 *  \return The char length (strlen) of the result string or -1 for failure
	 */
	WINPR_API SSIZE_T Stream_Read_UTF16_String_As_UTF8_Buffer(wStream* s, size_t wcharLength,
	                                                          char* utfBuffer,
	                                                          size_t utfBufferCharLength);

	/** \brief Writes a UTF-8 string UTF16 encoded to the stream. If the UTF-8
	 *  string is short, the remaining characters are filled up with '\0'
	 *
	 *  \param s The stream to write to
	 *  \param wcharLength the length (in WCHAR characters) to write
	 *  \param src The source data buffer with the UTF-8 data
	 *  \param length The length in bytes of the UTF-8 buffer
	 *  \param fill If \b TRUE fill the unused parts of the wcharLength with 0
	 *
	 *  \b return number of used characters for success, /b -1 for failure
	 */
	WINPR_API SSIZE_T Stream_Write_UTF16_String_From_UTF8(wStream* s, size_t wcharLength,
	                                                      const char* src, size_t length,
	                                                      BOOL fill);

	/* StreamPool */

	WINPR_API void StreamPool_Return(wStreamPool* pool, wStream* s);

	WINPR_API wStream* StreamPool_Take(wStreamPool* pool, size_t size);

	WINPR_API void Stream_AddRef(wStream* s);
	WINPR_API void Stream_Release(wStream* s);

	WINPR_API wStream* StreamPool_Find(wStreamPool* pool, const BYTE* ptr);

	WINPR_API void StreamPool_Clear(wStreamPool* pool);

	WINPR_API void StreamPool_Free(wStreamPool* pool);

	WINPR_ATTR_MALLOC(StreamPool_Free, 1)
	WINPR_API wStreamPool* StreamPool_New(BOOL synchronized, size_t defaultSize);

	WINPR_API char* StreamPool_GetStatistics(wStreamPool* pool, char* buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_UTILS_STREAM_H */
