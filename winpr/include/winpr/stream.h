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

struct _wStream
{
	BYTE* buffer;
	BYTE* pointer;
	size_t length;
	size_t capacity;
};

typedef struct _wStream wStream;

WINPR_API void Stream_EnsureCapacity(wStream* s, size_t size);

WINPR_API wStream* Stream_New(BYTE* buffer, size_t size);
WINPR_API void Stream_Free(wStream* s, BOOL bFreeBuffer);

#define Stream_Read_UINT8(_s, _v) do { _v = \
	*_s->pointer++; } while (0)

#define Stream_Read_UINT16(_s, _v) do { _v = \
	(UINT16)(*_s->pointer) + \
	(((UINT16)(*(_s->pointer + 1))) << 8); \
	_s->pointer += 2; } while (0)

#define Stream_Read_UINT16_BE(_s, _v) do { _v = \
	(((UINT16)(*_s->pointer)) << 8) + \
	(UINT16)(*(_s->pointer + 1)); \
	_s->pointer += 2; } while (0)

#define Stream_Read_UINT32(_s, _v) do { _v = \
	(UINT32)(*_s->pointer) + \
	(((UINT32)(*(_s->pointer + 1))) << 8) + \
	(((UINT32)(*(_s->pointer + 2))) << 16) + \
	(((UINT32)(*(_s->pointer + 3))) << 24); \
	_s->pointer += 4; } while (0)

#define Stream_Read_UINT32_BE(_s, _v) do { _v = \
	(((uint32)(*(_s->pointer))) << 24) + \
	(((uint32)(*(_s->pointer + 1))) << 16) + \
	(((uint32)(*(_s->pointer + 2))) << 8) + \
	(((uint32)(*(_s->pointer + 3)))); \
	_s->pointer += 4; } while (0)

#define Stream_Read_UINT64(_s, _v) do { _v = \
	(UINT64)(*_s->pointer) + \
	(((UINT64)(*(_s->pointer + 1))) << 8) + \
	(((UINT64)(*(_s->pointer + 2))) << 16) + \
	(((UINT64)(*(_s->pointer + 3))) << 24) + \
	(((UINT64)(*(_s->pointer + 4))) << 32) + \
	(((UINT64)(*(_s->pointer + 5))) << 40) + \
	(((UINT64)(*(_s->pointer + 6))) << 48) + \
	(((UINT64)(*(_s->pointer + 7))) << 56); \
	_s->pointer += 8; } while (0)

#define Stream_Read(_s, _b, _n) do { \
	memcpy(_b, (_s->pointer), (_n)); \
	_s->pointer += (_n); \
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

#define Stream_Peek_UINT8(_s, _v) do { _v = \
	*_s->pointer; } while (0)

#define Stream_Peek_UINT16(_s, _v) do { _v = \
	(UINT16)(*_s->pointer) + \
	(((UINT16)(*(_s->pointer + 1))) << 8); \
	} while (0)

#define Stream_Peek_UINT32(_s, _v) do { _v = \
	(UINT32)(*_s->pointer) + \
	(((UINT32)(*(_s->pointer + 1))) << 8) + \
	(((UINT32)(*(_s->pointer + 2))) << 16) + \
	(((UINT32)(*(_s->pointer + 3))) << 24); \
	} while (0)

#define Stream_Peek_UINT64(_s, _v) do { _v = \
	(UINT64)(*_s->pointer) + \
	(((UINT64)(*(_s->pointer + 1))) << 8) + \
	(((UINT64)(*(_s->pointer + 2))) << 16) + \
	(((UINT64)(*(_s->pointer + 3))) << 24) + \
	(((UINT64)(*(_s->pointer + 4))) << 32) + \
	(((UINT64)(*(_s->pointer + 5))) << 40) + \
	(((UINT64)(*(_s->pointer + 6))) << 48) + \
	(((UINT64)(*(_s->pointer + 7))) << 56); \
	} while (0)

#define Stream_Peek(_s, _b, _n) do { \
	memcpy(_b, (_s->pointer), (_n)); \
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

#define Stream_Pointer(_s)		_s->pointer
#define Stream_Buffer(_s)		_s->buffer
#define Stream_Length(_s)		_s->length
#define Stream_Capacity(_s)		_s->capacity
#define Stream_Position(_s)		(_s->pointer - _s->buffer)

#define Stream_SetPosition(_s, _p)	_s->pointer = _s->buffer + (_p)

#endif /* WINPR_UTILS_STREAM_H */
