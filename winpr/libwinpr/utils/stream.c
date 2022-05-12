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

#include <winpr/config.h>

#include <winpr/assert.h>
#include <winpr/crt.h>
#include <winpr/stream.h>

#include "stream.h"
#include "../log.h"

#define STREAM_TAG WINPR_TAG("wStream")

#define STREAM_ASSERT(cond)                                                                \
	do                                                                                     \
	{                                                                                      \
		if (!(cond))                                                                       \
		{                                                                                  \
			WLog_FATAL(STREAM_TAG, "%s [%s:%s:%" PRIuz "]", #cond, __FILE__, __FUNCTION__, \
			           __LINE__);                                                          \
			winpr_log_backtrace(STREAM_TAG, WLOG_FATAL, 20);                               \
			abort();                                                                       \
		}                                                                                  \
	} while (0)

BOOL Stream_EnsureCapacity(wStream* s, size_t size)
{
	WINPR_ASSERT(s);
	if (s->capacity < size)
	{
		size_t position;
		size_t old_capacity;
		size_t new_capacity;
		BYTE* new_buf;

		old_capacity = s->capacity;
		new_capacity = old_capacity;

		do
		{
			new_capacity *= 2;
		} while (new_capacity < size);

		position = Stream_GetPosition(s);

		if (!s->isOwner)
		{
			new_buf = (BYTE*)malloc(new_capacity);
			CopyMemory(new_buf, s->buffer, s->capacity);
			s->isOwner = TRUE;
		}
		else
		{
			new_buf = (BYTE*)realloc(s->buffer, new_capacity);
		}

		if (!new_buf)
			return FALSE;
		s->buffer = new_buf;
		s->capacity = new_capacity;
		s->length = new_capacity;
		ZeroMemory(&s->buffer[old_capacity], s->capacity - old_capacity);

		Stream_SetPosition(s, position);
	}
	return TRUE;
}

BOOL Stream_EnsureRemainingCapacity(wStream* s, size_t size)
{
	if (Stream_GetPosition(s) + size > Stream_Capacity(s))
		return Stream_EnsureCapacity(s, Stream_Capacity(s) + size);
	return TRUE;
}

wStream* Stream_New(BYTE* buffer, size_t size)
{
	wStream* s;

	if (!buffer && !size)
		return NULL;

	s = malloc(sizeof(wStream));
	if (!s)
		return NULL;

	if (buffer)
		s->buffer = buffer;
	else
		s->buffer = (BYTE*)malloc(size);

	if (!s->buffer)
	{
		free(s);
		return NULL;
	}

	s->pointer = s->buffer;
	s->capacity = size;
	s->length = size;

	s->pool = NULL;
	s->count = 0;
	s->isAllocatedStream = TRUE;
	s->isOwner = TRUE;
	return s;
}

wStream* Stream_StaticConstInit(wStream* s, const BYTE* buffer, size_t size)
{
	union
	{
		BYTE* b;
		const BYTE* cb;
	} cnv;

	cnv.cb = buffer;
	return Stream_StaticInit(s, cnv.b, size);
}

wStream* Stream_StaticInit(wStream* s, BYTE* buffer, size_t size)
{
	const wStream empty = { 0 };

	WINPR_ASSERT(s);
	WINPR_ASSERT(buffer);

	*s = empty;
	s->buffer = s->pointer = buffer;
	s->capacity = s->length = size;
	s->pool = NULL;
	s->count = 0;
	s->isAllocatedStream = FALSE;
	s->isOwner = FALSE;
	return s;
}

void Stream_EnsureValidity(wStream* s)
{
	size_t cur;

	STREAM_ASSERT(s);
	STREAM_ASSERT(s->pointer >= s->buffer);

	cur = (size_t)(s->pointer - s->buffer);
	STREAM_ASSERT(cur <= s->capacity);
	STREAM_ASSERT(s->length <= s->capacity);
}

void Stream_Free(wStream* s, BOOL bFreeBuffer)
{
	if (s)
	{
		Stream_EnsureValidity(s);
		if (bFreeBuffer && s->isOwner)
			free(s->buffer);

		if (s->isAllocatedStream)
			free(s);
	}
}

BOOL Stream_SetLength(wStream* _s, size_t _l)
{
	if ((_l) > Stream_Capacity(_s))
	{
		_s->length = 0;
		return FALSE;
	}
	_s->length = _l;
	return TRUE;
}

BOOL Stream_SetPosition(wStream* _s, size_t _p)
{
	if ((_p) > Stream_Capacity(_s))
	{
		_s->pointer = _s->buffer;
		return FALSE;
	}
	_s->pointer = _s->buffer + (_p);
	return TRUE;
}

void Stream_SealLength(wStream* _s)
{
	size_t cur;
	WINPR_ASSERT(_s);
	WINPR_ASSERT(_s->buffer <= _s->pointer);
	cur = (size_t)(_s->pointer - _s->buffer);
	WINPR_ASSERT(cur <= _s->capacity);
	if (cur <= _s->capacity)
		_s->length = cur;
	else
	{
		WLog_FATAL(STREAM_TAG, "wStream API misuse: stream was written out of bounds");
		winpr_log_backtrace(STREAM_TAG, WLOG_FATAL, 20);
		_s->length = 0;
	}
}

#if defined(WITH_WINPR_DEPRECATED)
BOOL Stream_SetPointer(wStream* _s, BYTE* _p)
{
	WINPR_ASSERT(_s);
	if (!_p || (_s->buffer > _p) || (_s->buffer + _s->capacity < _p))
	{
		_s->pointer = _s->buffer;
		return FALSE;
	}
	_s->pointer = _p;
	return TRUE;
}

BOOL Stream_SetBuffer(wStream* _s, BYTE* _b)
{
	WINPR_ASSERT(_s);
	WINPR_ASSERT(_b);

	_s->buffer = _b;
	_s->pointer = _b;
	return _s->buffer != NULL;
}

void Stream_SetCapacity(wStream* _s, size_t _c)
{
	WINPR_ASSERT(_s);
	_s->capacity = _c;
}

#endif

size_t Stream_GetRemainingCapacity(const wStream* _s)
{
	size_t cur;
	WINPR_ASSERT(_s);
	WINPR_ASSERT(_s->buffer <= _s->pointer);
	cur = (size_t)(_s->pointer - _s->buffer);
	WINPR_ASSERT(cur <= _s->capacity);
	if (cur > _s->capacity)
	{
		WLog_FATAL(STREAM_TAG, "wStream API misuse: stream was written out of bounds");
		winpr_log_backtrace(STREAM_TAG, WLOG_FATAL, 20);
		return 0;
	}
	return (_s->capacity - cur);
}

size_t Stream_GetRemainingLength(const wStream* _s)
{
	size_t cur;
	WINPR_ASSERT(_s);
	WINPR_ASSERT(_s->buffer <= _s->pointer);
	WINPR_ASSERT(_s->length <= _s->capacity);
	cur = (size_t)(_s->pointer - _s->buffer);
	WINPR_ASSERT(cur <= _s->length);
	if (cur > _s->length)
	{
		WLog_FATAL(STREAM_TAG, "wStream API misuse: stream was read out of bounds");
		winpr_log_backtrace(STREAM_TAG, WLOG_FATAL, 20);
		return 0;
	}
	return (_s->length - cur);
}

BOOL Stream_Write_UTF16_String(wStream* s, const WCHAR* src, size_t length)
{
	size_t x;

	WINPR_ASSERT(s);
	WINPR_ASSERT(src || (length == 0));
	if (!s || !src)
		return FALSE;

	if (Stream_GetRemainingCapacity(s) / sizeof(WCHAR) < length)
		return FALSE;

	for (x = 0; x < length; x++)
		Stream_Write_UINT16(s, src[x]);

	return TRUE;
}

BOOL Stream_Read_UTF16_String(wStream* s, WCHAR* dst, size_t length)
{
	size_t x;

	WINPR_ASSERT(s);
	WINPR_ASSERT(dst);

	if (Stream_GetRemainingLength(s) / sizeof(WCHAR) < length)
		return FALSE;

	for (x = 0; x < length; x++)
		Stream_Read_UINT16(s, dst[x]);

	return TRUE;
}

BOOL Stream_CheckAndLogRequiredLengthEx(const char* tag, DWORD level, wStream* s, UINT64 len,
                                        const char* fmt, ...)
{
	const size_t actual = Stream_GetRemainingLength(s);

	if (actual < len)
	{
		va_list args;

		va_start(args, fmt);
		Stream_CheckAndLogRequiredLengthExVa(tag, level, s, len, fmt, args);
		va_end(args);

		return FALSE;
	}
	return TRUE;
}

BOOL Stream_CheckAndLogRequiredLengthExVa(const char* tag, DWORD level, wStream* s, UINT64 len,
                                          const char* fmt, va_list args)
{
	const size_t actual = Stream_GetRemainingLength(s);

	if (actual < len)
		return Stream_CheckAndLogRequiredLengthWLogExVa(WLog_Get(tag), level, s, len, fmt, args);
	return TRUE;
}

BOOL Stream_CheckAndLogRequiredLengthWLogEx(wLog* log, DWORD level, wStream* s, UINT64 len,
                                            const char* fmt, ...)
{
	const size_t actual = Stream_GetRemainingLength(s);

	if (actual < len)
	{
		va_list args;

		va_start(args, fmt);
		Stream_CheckAndLogRequiredLengthWLogExVa(log, level, s, len, fmt, args);
		va_end(args);

		return FALSE;
	}
	return TRUE;
}

BOOL Stream_CheckAndLogRequiredLengthWLogExVa(wLog* log, DWORD level, wStream* s, UINT64 len,
                                              const char* fmt, va_list args)
{
	const size_t actual = Stream_GetRemainingLength(s);

	if (actual < len)
	{
		char prefix[1024] = { 0 };

		vsnprintf(prefix, sizeof(prefix), fmt, args);

		WLog_Print(log, level, "[%s] invalid length, got %" PRIuz ", require at least %" PRIu64,
		           prefix, actual, len);
		winpr_log_backtrace_ex(log, level, 20);
		return FALSE;
	}
	return TRUE;
}
